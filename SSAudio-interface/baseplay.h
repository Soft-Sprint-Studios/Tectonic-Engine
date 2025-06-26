/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */

// The SSAudio Interface Wrapper
/*
*   SSAudio is composed of three parts:
*   - game linker
*   - interface wrapper (you are here)
*   - API plugin
* 
*   The Interface Wrapper contains the base classes 
*   for adding various audio plugins, 
*   which can be made by the community and us!
*/
#ifndef BASEPLAY_H
#define BASEPLAY_H
#endif //BASEPLAY_H


#ifdef __cplusplus
extern "C" {
#endif

    typedef struct {
        unsigned int bufferID;
    } Sound;

    typedef struct {
        unsigned int sourceID;
    } PlayingSound;

#ifdef __cplusplus
}
#endif


// CLASSNAME: CBaseAudioPlayer
// PURPOSE:  Serve as a base class for the rest of audio players.
class CBaseAudioPlayer
{
public:
    unsigned int PlaySound();
};