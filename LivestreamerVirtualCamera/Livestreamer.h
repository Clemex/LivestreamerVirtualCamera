#pragma once

class Livestreamer {
public:
   Livestreamer();
   ~Livestreamer();
   HBITMAP GetNextFrame();
   int GetWidth();
   int GetHeight();
private:
   zmq::context_t context;
   zmq::socket_t socket;
   HBITMAP ConvertMatToBitmap(cv::Mat frame);
   void InitZMQ();
   cv::Mat GrabFrameFromZMQ();
};
