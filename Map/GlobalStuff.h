//const int  SQUARE_SIZE = 8;
#ifndef GS
#define GS
#define SAFE_NEW new
class gst{
	public:
		int mapx;
		int mapy;
		int hmapx;
		int hmapy;
		int frameNum;
		int mapSquares;
		int pwr2mapx;
		int pwr2mapy;
	
	};

extern gst* gs;
#endif
