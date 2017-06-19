#include "stdafx.h"
#include "Livestreamer.h"

Livestreamer::Livestreamer() : context(1), socket(context, ZMQ_SUB) 
{
   InitZMQ();
}

Livestreamer::~Livestreamer() 
{

}

HBITMAP Livestreamer::GetNextFrame()
{
   auto m = GrabFrameFromZMQ();
   return ConvertMatToBitmap(m);
}

HBITMAP Livestreamer::ConvertMatToBitmap(cv::Mat frame)
{
   auto imageSize = frame.size();
   assert(imageSize.width && "invalid size provided by frame");
   assert(imageSize.height && "invalid size provided by frame");

   if (imageSize.width && imageSize.height)
   {
      auto headerInfo = BITMAPINFOHEADER{};
      ZeroMemory(&headerInfo, sizeof(headerInfo));

      headerInfo.biSize = sizeof(headerInfo);
      headerInfo.biWidth = imageSize.width;
      headerInfo.biHeight = -(imageSize.height); // negative otherwise it will be upsidedown
      headerInfo.biPlanes = 1;// must be set to 1 as per documentation frame.channels();

      headerInfo.biBitCount = frame.channels() * 8;

      auto bitmapInfo = BITMAPINFO{};
      ZeroMemory(&bitmapInfo, sizeof(bitmapInfo));

      bitmapInfo.bmiHeader = headerInfo;
      bitmapInfo.bmiColors->rgbBlue = 0;
      bitmapInfo.bmiColors->rgbGreen = 0;
      bitmapInfo.bmiColors->rgbRed = 0;
      bitmapInfo.bmiColors->rgbReserved = 0;

      auto dc = GetDC(nullptr);
      assert(dc != nullptr && "Failure to get DC");
      auto bmp = CreateDIBitmap(dc,
         &headerInfo,
         CBM_INIT,
         frame.data,
         &bitmapInfo,
         DIB_RGB_COLORS);
      assert(bmp != nullptr && "Failure creating bitmap from captured frame");

      return bmp;
   }
   else
   {
      return nullptr;
   }
}

void Livestreamer::InitZMQ()
{
   //  Prepare our context and socket
   socket.setsockopt(ZMQ_RCVHWM, 1);
   socket.connect("tcp://127.0.0.1:5577");
   socket.setsockopt(ZMQ_SUBSCRIBE, "", 0);
}

cv::Mat Livestreamer::GrabFrameFromZMQ()
{
   zmq::message_t reply;
   socket.recv(&reply, 0);
   char* cc = new char[reply.size()];
   memcpy(cc, reply.data(), reply.size());
   delete cc;
   unsigned int height = cc[0] | cc[1] << 8;
   unsigned int width = cc[2] | cc[3] << 8;
   unsigned int channels = cc[4];
   auto m = cv::Mat(height, width, CV_8UC(channels), &cc[5]);
   return m;
}

int getNegotiatedFinalWidth() {
   return 0;
}

int getNegotiatedFinalHeight() {
   return 0;
}

int getCaptureDesiredFinalWidth() {
   return 0;
}

int getCaptureDesiredFinalHeight() {
   return 0;
}
