#include "baseplay.h"

CBaseAudioPlayer::CBaseAudioPlayer()
{
	//constructor, nothing at the moment
}

unsigned int CBaseAudioPlayer::PlaySound(unsigned int bufferID, float volume, float pitch, float maxDistance, bool looping)
{
	return 0; //defaults.
}

void CBaseAudioPlayer::UnsetBuffer(unsigned int bufferID)
{
}

void CBaseAudioPlayer::UnsetSource(unsigned int sourceID)
{
}
