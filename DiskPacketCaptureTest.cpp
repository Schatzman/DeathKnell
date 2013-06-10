
#include "DiskPacketCaptureTest.h"
#include "MockConf.h"
#include "dpi/modules/upacket.h"
#include "pcap/pcap.h"
#include <thread>
#include <future>

TEST_F(DiskPacketCaptureTest, Construct) {
#ifdef LR_DEBUG
   MockConf conf;
   DiskPacketCapture capture(conf);
   DiskPacketCapture* pCapture = new DiskPacketCapture(conf);
   delete pCapture;
#endif
}

TEST_F(DiskPacketCaptureTest, GetRunningPackets) {
#ifdef LR_DEBUG
   MockConf conf;
   MockDiskPacketCapture capture(conf);

   std::stringstream testDir;
   testDir << "/tmp/TooMuchPcap." << pthread_self();

   conf.mPCapCaptureLocation = testDir.str();

   std::string makeADir = "mkdir -p ";
   makeADir += testDir.str();

   ASSERT_EQ(0, system(makeADir.c_str()));
   conf.mPCapCaptureFileLimit = 10000;
   conf.mPCapCaptureSizeLimit = 10000;
   ASSERT_TRUE(capture.Initialize());
   std::pair<InMemoryPacketBuffer*, size_t>* sessionInfo;
   std::pair<InMemoryPacketBuffer*, size_t>* sessionInfo2;
   capture.GetRunningPackets("abc123", sessionInfo);
   capture.GetRunningPackets("abc123", sessionInfo2);
   ASSERT_EQ(sessionInfo, sessionInfo2);
   capture.GetRunningPackets("def456", sessionInfo2);
   ASSERT_NE(sessionInfo, sessionInfo2);

   //A promise object that can be used for sharing a variable between threads
   std::promise<std::pair<InMemoryPacketBuffer*, size_t>* > promisedPacket;

   // A future variable that reads from the promise, allows you to wait on the other thread
   std::future<std::pair<InMemoryPacketBuffer*, size_t>* > futurePacket = promisedPacket.get_future();

   // A thread that does a call to the capture object, stores the result in the promise.
   //
   //  This uses what is refered to as a lambda function ( expressed as [] ).  This is a full function
   // Defined as an L-Value of call.  These are broken down as follows:
   // [] ( Argument definitions ) { Normal C++ code }
   //
   // The remaining arguments of the thread constructor are the list of arguments.  using std::ref you allow
   // the arguments to be references rather than pointers in this context.  
   //
   // Finally the .detach() breaks the thread off to execute in the background
   std::thread([](std::promise<std::pair<InMemoryPacketBuffer*, size_t>* >& pointer,
           MockDiskPacketCapture & capture) {
      std::pair<InMemoryPacketBuffer*, size_t>* sessionInfo3;
      capture.GetRunningPackets("abc123", sessionInfo3);
              pointer.set_value(sessionInfo3); }, std::ref(promisedPacket), std::ref(capture)).detach();

   // Wait for the thread to set the promise object to a new value
   futurePacket.wait();

   // Verify that the code run in the thread gives you a different pointer 
   capture.GetRunningPackets("abc123", sessionInfo);
   std::pair<InMemoryPacketBuffer*, size_t>* otherRunningPacket = futurePacket.get();
   EXPECT_NE(sessionInfo, otherRunningPacket);
   makeADir = "";
   makeADir = "rm -rf ";
   makeADir += testDir.str();
   ASSERT_EQ(0, system(makeADir.c_str()));
#endif
}

TEST_F(DiskPacketCaptureTest, GetFilenamesTest) {
#ifdef LR_DEBUG
   MockConf conf;
   MockDiskPacketCapture capture(conf);
   
   conf.mPCapCaptureLocation = "testLocation";
   std::time_t time = 123456789;
   std::string fileName = capture.BuildFilename("TestUUID", "TestAppName", "24.24.24.24", "255.255.255.255", time);
   ASSERT_EQ("testLocation/TestUUID|TestAppName|24.24.24.24|255.255.255.255|1973-11-29-21:33:09", fileName);
#endif
}

TEST_F(DiskPacketCaptureTest, Initialize) {
#ifdef LR_DEBUG
   MockConf conf;
   MockDiskPacketCapture capture(conf);

   std::stringstream testDir;
   testDir << "/tmp/TooMuchPcap." << pthread_self();

   std::string makeADir = "mkdir -p ";
   makeADir += testDir.str();
   ASSERT_EQ(0, system(makeADir.c_str()));
   conf.mPCapCaptureLocation = "testLocation";
   ASSERT_FALSE(capture.Initialize());
   conf.mPCapCaptureLocation = testDir.str();
   ASSERT_TRUE(capture.Initialize());
   conf.mPCapCaptureLocation = "/etc/passwd";
   ASSERT_FALSE(capture.Initialize());
   makeADir = "";
   makeADir = "rm -rf ";
   makeADir += testDir.str();
   ASSERT_EQ(0, system(makeADir.c_str()));
#endif 
}

TEST_F(DiskPacketCaptureTest, TooMuchPCap) {
#ifdef LR_DEBUG
   if (geteuid() == 0) {
      MockConf conf;
      MockDiskPacketCapture capture(conf);

      conf.mPCapCaptureLocation = "testLocation";
      conf.mPCapCaptureFileLimit = 10000;
      conf.mPCapCaptureSizeLimit = 10000;
      ASSERT_FALSE(capture.Initialize());
      std::atomic<size_t> aDiskUsed;
      std::atomic<size_t> aTotalFiles;
      capture.RecalculateDiskUsed(aDiskUsed,aTotalFiles);
      ASSERT_FALSE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));

      std::stringstream testDir;
      testDir << "/tmp/TooMuchPcap." << pthread_self();

      conf.mPCapCaptureLocation = testDir.str();

      std::string makeADir = "mkdir -p ";
      makeADir += testDir.str();

      ASSERT_EQ(0, system(makeADir.c_str()));
capture.RecalculateDiskUsed(aDiskUsed,aTotalFiles);
      EXPECT_TRUE(capture.Initialize());
      EXPECT_FALSE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      conf.mPCapCaptureFileLimit = 0;
      EXPECT_FALSE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      conf.mPCapCaptureFileLimit = 10000;
      conf.mPCapCaptureSizeLimit = 0;
      EXPECT_FALSE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      conf.mPCapCaptureFileLimit = 1;
      conf.mPCapCaptureSizeLimit = 1;
      EXPECT_FALSE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      std::string makeSmallFile = "touch ";
      makeSmallFile += testDir.str();
      makeSmallFile += "/smallFile";

      EXPECT_EQ(0, system(makeSmallFile.c_str()));
      std::this_thread::sleep_for(std::chrono::seconds(1));
      capture.RecalculateDiskUsed(aDiskUsed,aTotalFiles);
      EXPECT_TRUE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      conf.mPCapCaptureFileLimit = 10;
      EXPECT_FALSE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      std::string make1MFileFile = "dd bs=1024 count=1024 if=/dev/zero of=";
      make1MFileFile += testDir.str();
      make1MFileFile += "/1MFile";


      EXPECT_EQ(0, system(make1MFileFile.c_str()));
      std::this_thread::sleep_for(std::chrono::seconds(1));
      capture.RecalculateDiskUsed(aDiskUsed,aTotalFiles);
      EXPECT_TRUE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      conf.mPCapCaptureSizeLimit = 3;
      EXPECT_FALSE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      make1MFileFile = "dd bs=1048576 count=1 if=/dev/zero of=";
      make1MFileFile += testDir.str();
      make1MFileFile += "/1MFilelessone";
      EXPECT_EQ(0, system(make1MFileFile.c_str()));
      std::this_thread::sleep_for(std::chrono::seconds(1));
      capture.RecalculateDiskUsed(aDiskUsed,aTotalFiles);
      EXPECT_FALSE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));

      conf.mPCapCaptureFileLimit = 3;
      EXPECT_TRUE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      
      capture.RemoveOldestPCapFile();
      capture.RecalculateDiskUsed(aDiskUsed,aTotalFiles);
      EXPECT_FALSE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      conf.mPCapCaptureSizeLimit = 1;
      EXPECT_TRUE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      capture.RemoveOldestPCapFile();
      capture.RecalculateDiskUsed(aDiskUsed,aTotalFiles);
      EXPECT_FALSE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      conf.mPCapCaptureFileLimit = 1;
      EXPECT_TRUE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      capture.RemoveOldestPCapFile();
      capture.RecalculateDiskUsed(aDiskUsed,aTotalFiles);
      EXPECT_FALSE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));

      makeADir = "";
      makeADir = "rm -rf ";
      makeADir += testDir.str();
      ASSERT_EQ(0, system(makeADir.c_str()));
      ASSERT_FALSE(capture.Initialize());
   }
#endif
}

TEST_F(DiskPacketCaptureTest, CleanupOldPcapFiles) {
#ifdef LR_DEBUG
   if (geteuid() == 0) {
      MockConf conf;
      MockDiskPacketCapture capture(conf);
      std::atomic<size_t> aDiskUsed;
      std::atomic<size_t> aTotalFiles;
      conf.mPCapCaptureLocation = "testLocation";
      conf.mPCapCaptureFileLimit = 10000;
      conf.mPCapCaptureSizeLimit = 10000;
      ASSERT_FALSE(capture.Initialize());
      capture.RecalculateDiskUsed(aDiskUsed,aTotalFiles);
      ASSERT_FALSE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));

      std::stringstream testDir;
      testDir << "/tmp/TooMuchPcap." << pthread_self();

      conf.mPCapCaptureLocation = testDir.str();

      std::string makeADir = "mkdir -p ";
      makeADir += testDir.str();

      ASSERT_EQ(0, system(makeADir.c_str()));

      EXPECT_TRUE(capture.Initialize());
      std::string makeSmallFile = "touch ";
      makeSmallFile += testDir.str();
      makeSmallFile += "/smallFile";
      EXPECT_EQ(0, system(makeSmallFile.c_str()));
      std::this_thread::sleep_for(std::chrono::seconds(1));
      capture.RecalculateDiskUsed(aDiskUsed,aTotalFiles);
      std::string make1MFileFile = "dd bs=1024 count=1024 if=/dev/zero of=";
      make1MFileFile += testDir.str();
      make1MFileFile += "/1MFile";
      EXPECT_EQ(0, system(make1MFileFile.c_str()));
      std::this_thread::sleep_for(std::chrono::seconds(1));
      capture.RecalculateDiskUsed(aDiskUsed,aTotalFiles);
      make1MFileFile = "dd bs=1048576 count=1 if=/dev/zero of=";
      make1MFileFile += testDir.str();
      make1MFileFile += "/1MFilelessone";
      EXPECT_EQ(0, system(make1MFileFile.c_str()));
      std::this_thread::sleep_for(std::chrono::seconds(1));
      capture.RecalculateDiskUsed(aDiskUsed,aTotalFiles);
      conf.mPCapCaptureFileLimit = 3;
      EXPECT_TRUE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      capture.CleanupOldPcapFiles(aDiskUsed,aTotalFiles);
      EXPECT_FALSE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      conf.mPCapCaptureSizeLimit = 1;
      EXPECT_TRUE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      capture.CleanupOldPcapFiles(aDiskUsed,aTotalFiles);
      EXPECT_FALSE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      conf.mPCapCaptureFileLimit = 1;
      EXPECT_TRUE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));
      capture.CleanupOldPcapFiles(aDiskUsed,aTotalFiles);
      EXPECT_FALSE(capture.TooMuchPCap(aDiskUsed,aTotalFiles));

      makeADir = "";
      makeADir = "rm -rf ";
      makeADir += testDir.str();
      ASSERT_EQ(0, system(makeADir.c_str()));
      ASSERT_FALSE(capture.Initialize());
   }
#endif
}

TEST_F(DiskPacketCaptureTest, MemoryLimits) {
#ifdef LR_DEBUG
   MockConf conf;
   MockDiskPacketCapture capture(conf);
   conf.mPCapCaptureLocation = "testLocation";
   conf.mPCapCaptureFileLimit = 10000;
   conf.mPCapCaptureSizeLimit = 10000;
   conf.mPCapCaptureMemoryLimit = 10;
   std::stringstream testDir;
   testDir << "/tmp/TooMuchPcap." << pthread_self();

   conf.mPCapCaptureLocation = testDir.str();

   std::string makeADir = "mkdir -p ";
   makeADir += testDir.str();

   ASSERT_EQ(0, system(makeADir.c_str()));
   ASSERT_TRUE(capture.Initialize());

   EXPECT_EQ(0, capture.NewTotalMemory(0));
   EXPECT_EQ(0, capture.CurrentMemoryForFlow("notThere"));
   struct upacket packet;
   ctb_pkt p;
   packet.p = &p;
   unsigned char data[(1024 * 1024) - sizeof (struct pcap_pkthdr)];
   p.len = (1024 * 1024) - sizeof (struct pcap_pkthdr);
   p.data = data;
   capture.SavePacket("FlowOne", &packet);
   EXPECT_EQ(1, capture.NewTotalMemory(0));
   EXPECT_EQ(1, capture.CurrentMemoryForFlow("FlowOne"));
   p.len = (1024 * 1024) - sizeof (struct pcap_pkthdr) - sizeof (struct pcap_pkthdr) - 1;
   capture.SavePacket("FlowOne", &packet);
   EXPECT_EQ(1, capture.NewTotalMemory(0));
   EXPECT_EQ(1, capture.CurrentMemoryForFlow("FlowOne"));
   p.len = 1;
   capture.SavePacket("FlowTwo", &packet);
   EXPECT_EQ(2, capture.NewTotalMemory(0));
   EXPECT_EQ(1, capture.CurrentMemoryForFlow("FlowOne"));
   EXPECT_EQ(0, capture.CurrentMemoryForFlow("FlowTwo"));

   conf.mPCapCaptureMemoryLimit = 2;
   p.len = (1024 * 1024) - sizeof (struct pcap_pkthdr);
   capture.SavePacket("FlowTwo", &packet);
   EXPECT_EQ(2, capture.NewTotalMemory(0));
   EXPECT_EQ(1, capture.CurrentMemoryForFlow("FlowOne"));
   EXPECT_EQ(0, capture.CurrentMemoryForFlow("FlowTwo"));

   conf.mPCapCaptureMemoryLimit = 3;
   EXPECT_EQ(3, capture.NewTotalMemory((1024 * 1024)));

   capture.RemoveDataFromRunningPackets("FlowOne");
   EXPECT_EQ(0, capture.NewTotalMemory(0));
   EXPECT_EQ(0, capture.CurrentMemoryForFlow("FlowOne"));

   makeADir = "rm -rf ";
   makeADir += testDir.str();
   ASSERT_EQ(0, system(makeADir.c_str()));
   ASSERT_FALSE(capture.Initialize());
#endif
}