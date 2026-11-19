/*!
    iltlt2lightanims.h: the light animation table of the world last loaded, for a LithTech 2.x game's ILTLightAnim.

    LT2 baked its switchable lights into the world file as named light animations, each one or more frames of
    per polygon lightmaps, and let game code find and blend them by name. Jupiter has light groups instead.
    The world loader turns a v66 world's animations into light groups as it converts the world, and publishes
    the animations themselves here in file order, so an index is LT2's HLIGHTANIM:

        light group "LT2LA_<anim>_<frame>_<R|G|B>"   one channel of one frame

    Only the engine implements this. The server and the client load the world through the same converter, so in
    one process the table is the world both are in. The serial changes on every publish, so a consumer can tell
    a new world from the old one. The table is empty after loading any world that isn't v66.
*/

#ifndef __ILTLT2LIGHTANIMS_H__
#define __ILTLT2LIGHTANIMS_H__

#ifndef __LTMODULE_H__
#include "ltmodule.h"
#endif

class ILTLT2LightAnims : public IBase
{
public:
    interface_version(ILTLT2LightAnims, 0);

    //! Changes every time a world load publishes a table, empty or not
    virtual uint32 GetSerial() = 0;

    //! The number of light animations, LightAnim_BASE (index 0) included
    virtual uint32 GetCount() = 0;

    //! Animation nIndex: its name, frame count, and whether it is shadow mapped.
    //! The base and shadow mapped animations are listed but carry no light groups.
    //! False for an index out of range.
    virtual bool GetAnim(uint32 nIndex, const char **ppName, uint32 *pnFrames, bool *pbShadowMap) = 0;
};

#endif // __ILTLT2LIGHTANIMS_H__
