#pragma once

class Livestreamer {
public:
   Livestreamer();
   ~Livestreamer();
   HBITMAP GetNextFrame();
   int GetWidth();
   int GetHeight();
private:
   char* memory_buffer;
   long memory_buffer_size;
   zmq::context_t context;
   zmq::socket_t socket;
   cv::Mat last_frame;
   HBITMAP ConvertMatToBitmap(cv::Mat frame);
   void InitZMQ();
   cv::Mat GrabFrameFromZMQ();
};
