#include "stdafx.h"
#include "Livestreamer.h"

Livestreamer::Livestreamer() : context(1), socket(context, ZMQ_SUB), memory_buffer(NULL), last_frame(), last_bitmap()
{
   bitmap_dc = GetDC(nullptr);
   InitZMQ();
}

Livestreamer::~Livestreamer() 
{
   socket.close();
   context.close();
   delete memory_buffer;
   last_frame.release();
   DeleteObject(bitmap_dc);
   DeleteObject(last_bitmap);
}

HBITMAP Livestreamer::GetNextFrame()
{
   auto m = GrabFrameFromZMQ();
   return ConvertMatToBitmap(m);
}

HBITMAP Livestreamer::ConvertMatToBitmap(cv::Mat frame)
{
   auto imageSize = frame.size();

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

      auto bmp = CreateDIBitmap(bitmap_dc,
         &headerInfo,
         CBM_INIT,
         frame.data,
         &bitmapInfo,
         DIB_RGB_COLORS);

      DeleteObject(last_bitmap);
      last_bitmap = bmp;
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
   GrabFrameFromZMQ();
}

cv::Mat Livestreamer::GrabFrameFromZMQ()
{
   zmq::message_t reply;
   socket.recv(&reply, 0);
   if (reply.size() != memory_buffer_size)
   {
      if (memory_buffer != NULL)
         delete memory_buffer;
      memory_buffer_size = reply.size();
      memory_buffer = new unsigned char[memory_buffer_size];
   }
   memcpy(memory_buffer, reply.data(), reply.size());
   unsigned int height = memory_buffer[0] | memory_buffer[1] << 8;
   unsigned int width = memory_buffer[2] | memory_buffer[3] << 8;
   unsigned int channels = memory_buffer[4];
   auto m = cv::Mat(height, width, CV_8UC(channels), &memory_buffer[5]);
   last_frame.release();
   last_frame = m;
   return m;
}

int Livestreamer::GetHeight()
{
   return last_frame.rows;
}

int Livestreamer::GetWidth()
{
   return last_frame.cols;
}
