#include <irrlicht.h>

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;

int main()
{

	IrrlichtDevice* device =
		createDevice(video::EDT_OPENGL, dimension2d<u32>(800, 600), 16,
			false, false, false, 0);


	while (device->run())
	{
	}

	return 0;
}

