/*
    DECompat lets LithTech 1.0 era game code compile and run against
    the Jupiter engine without being rewritten first.

    Nothing in this layer changes a data representation.
    Every mapping is a rename onto the Jupiter primitive 
    the converted client shell uses on the other end of the wire.

    This is the server half. The client half (de_client.h, de_clientshell.h) isn't included here,
    as a server module must not see ILTClient.
    Client shells reach it through LT1's client include names in lt1/.
*/

#ifndef __DE_COMPAT_H__
#define __DE_COMPAT_H__

#include "de_types.h"
#include "de_message.h"

// A client module gets only those two. Blood 2's client shell reaches this file through basedefs_de.h,
// and GCC would emit DEBaseClass's inline virtuals there, which need server symbols a client can't link.
// LIB_DECompatClient defines DECOMPAT_CLIENT_MODULE for every module that links it
#ifndef DECOMPAT_CLIENT_MODULE
#include "de_server.h"
#include "de_engineobjects.h"
#include "de_servershell.h"
#else
// ObjectCreateStruct: LT1's basedefs_de.h declared it, and client code creates its effect objects with one
#include "ltobjectcreate.h"
#endif

#endif // __DE_COMPAT_H__