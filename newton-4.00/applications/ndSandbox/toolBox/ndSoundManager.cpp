/* Copyright (c) <2003-2021> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/

#include "ndSandboxStdafx.h"
#include "ndSoundManager.h"
#include "ndDemoEntityManager.h"

#if 0

void ndSoundManager::UpdateListener(const dVector& position, const dVector& velocity, const dVector& heading, const dVector& upDir)
{
	dAssert(0);
	/*
	dVector fposition (m_coordinateSystem.RotateVector(position));
	dVector fvelocity (m_coordinateSystem.RotateVector(velocity));
	dVector fheading (m_coordinateSystem.RotateVector(heading));
	dVector fupDir (m_coordinateSystem.RotateVector(upDir));

	FMOD_VECTOR up = { fupDir.m_x, fupDir.m_y, fupDir.m_z };
	FMOD_VECTOR pos = { fposition.m_x, fposition.m_y, fposition.m_z };
	FMOD_VECTOR vel = { fvelocity.m_x, fvelocity.m_y, fvelocity.m_z };
	FMOD_VECTOR forward = { fheading.m_x, fheading.m_y, fheading.m_z };

	FMOD_RESULT result = FMOD_System_Set3DListenerAttributes(m_system, 0, &pos, &vel, &forward, &up);
	ERRCHECK(result);
	*/
}

void ndSoundManager::DestroySound(void* const soundAssetHandle)
{
	dAssert(0);
	if (m_device)
	{
		ndSoundAssetList::dNode* const node = (ndSoundAssetList::dNode*)soundAssetHandle;
		dAssert(node);

		ndSoundAsset& asset = node->GetInfo();
		dAssert(asset.GetRef() > 1);
		asset.Release();

		m_assets.Remove(node);
	}
}

void ndSoundManager::DestroyAllSound()
{
	dAssert(0);
	if (m_device)
	{
		while (m_assets.GetRoot())
		{
			DestroySound(m_assets.GetRoot());
		}
	}
}

dFloat32 ndSoundManager::GetSoundlength(void* const soundAssetHandle)
{
	dAssert(0);
	if (m_device)
	{
		ndSoundAssetList::dNode* const node = (ndSoundAssetList::dNode*)soundAssetHandle;
		ndSoundAsset& asset = node->GetInfo();
		return 	asset.m_lenght;
	}
	return 0;
}


void ndSoundManager::DestroyChannel(void* const channelHandle)
{
	dAssert(0);
	if (m_device)
	{
		dAssert(0);
	}
}

void* ndSoundManager::GetAsset(void* const channelHandle) const
{
	dAssert(0);
	if (m_device)
	{
		ndSoundChannelList::dNode* const node = (ndSoundChannelList::dNode*)channelHandle;
		ndSoundChannel& channel = node->GetInfo();
		return channel.m_myAssetNode;
	}
	return nullptr;
}

#endif

ndSoundChannel::ndSoundChannel()
	:m_source(0)
	,m_asset(nullptr)
	,m_manager(nullptr)
	,m_playingNode(nullptr)
{
	alGenSources(1, (ALuint*)&m_source);
	dAssert(m_source);
	dAssert(alGetError() == AL_NO_ERROR);
}

ndSoundChannel::~ndSoundChannel()
{
	alDeleteSources(1, (ALuint*)&m_source);
	dAssert(alGetError() == AL_NO_ERROR);
}

bool ndSoundChannel::GetLoop() const
{
	ALint state;
	alGetSourcei(m_source, AL_LOOPING, &state);
	return state ? true : false;
}

void ndSoundChannel::SetLoop(bool mode)
{
	alSourcei(m_source, AL_LOOPING, mode ? 1 : 0);
}

bool ndSoundChannel::IsPlaying() const
{
	ALint state;
	alGetSourcei(m_source, AL_SOURCE_STATE, &state);
	return (state == AL_PLAYING) ? true : false;
}

void ndSoundChannel::Play()
{
	// set some default values
	if (!IsPlaying())
	{
		dAssert(!m_playingNode);
		alSourcef(m_source, AL_GAIN, 1);
		alSource3f(m_source, AL_POSITION, 0, 0, 0);
		alSource3f(m_source, AL_VELOCITY, 0, 0, 0);
		alSourcei(m_source, AL_LOOPING, AL_FALSE);

		alSourcePlay(m_source);
		dAssert(alGetError() == AL_NO_ERROR);
		m_playingNode = m_manager->m_channelPlaying.Append(this);
	}
}

void ndSoundChannel::Stop()
{
	if (IsPlaying())
	{
		alSourceStop(m_source);
		dAssert(alGetError() == AL_NO_ERROR);
	}
	if (m_playingNode)
	{
		m_manager->m_channelPlaying.Remove(m_playingNode);
		m_playingNode = nullptr;
	}
}

void ndSoundChannel::SetVolume(dFloat32 volume)
{
	ALfloat vol = ALfloat(volume);
	alSourcef(m_source, AL_GAIN, vol);
}

dFloat32 ndSoundChannel::GetVolume() const
{
	ALfloat volume;
	alGetSourcef(m_source, AL_GAIN, &volume);
	return volume;
}

dFloat32 ndSoundChannel::GetPitch() const
{
	ALfloat pitch;
	alGetSourcef(m_source, AL_PITCH, &pitch);
	return pitch;
}

void ndSoundChannel::SetPitch(dFloat32 pitch)
{
	ALfloat pit = ALfloat(pitch);
	alSourcef(m_source, AL_PITCH, pit);
}

dFloat32 ndSoundChannel::GetLength() const
{
	dAssert(0);
	return 0;
}

dFloat32 ndSoundChannel::GetSecPosition() const
{
	ALfloat position;
	alGetSourcef(m_source, AL_SEC_OFFSET, &position);
	return position;
}

ndSoundAsset::ndSoundAsset()
	:ndSoundChannelList()
	,m_buffer(0)
	,m_lenght(0)
	,m_frequecy(0)
	,m_node(nullptr)
{
	alGenBuffers(1, (ALuint*)&m_buffer);
	dAssert(m_buffer);
	dAssert(alGetError() == AL_NO_ERROR);
}

ndSoundAsset::ndSoundAsset(const ndSoundAsset& copy)
	:ndSoundChannelList()
	,m_buffer(copy.m_buffer)
	,m_lenght(copy.m_lenght)
	,m_frequecy(copy.m_frequecy)
	,m_node(nullptr)
{
	alGenBuffers(1, (ALuint*)&m_buffer);
	dAssert(m_buffer);
	dAssert(alGetError() == AL_NO_ERROR);
	dAssert(copy.GetCount() == 0);
}

ndSoundAsset::~ndSoundAsset()
{
	RemoveAll();
	alDeleteBuffers(1, (ALuint *)&m_buffer);
	dAssert(alGetError() == AL_NO_ERROR);
}

ndSoundManager::ndSoundManager()
	:ndModel()
	,m_device(alcOpenDevice(nullptr))
	,m_context(nullptr)
	,m_assets()
	,m_channelPlaying()
	//,m_coordinateSystem(dGetIdentityMatrix())
{
	dAssert(m_device);
	if (m_device)
	{
		m_context = alcCreateContext(m_device, nullptr);
		alcMakeContextCurrent(m_context);
	
		// clear error code
		//alGetError();
		dAssert(alGetError() == AL_NO_ERROR);
	
		ALfloat listenerPos[] = { 0.0,0.0,0.0 };
		alListenerfv(AL_POSITION, listenerPos);
		dAssert(alGetError() == AL_NO_ERROR);
	
		ALfloat listenerVel[] = { 0.0,0.0,0.0 };
		alListenerfv(AL_VELOCITY, listenerVel);
		dAssert(alGetError() == AL_NO_ERROR);
	
		ALfloat listenerOri[] = { 0.0,0.0,-1.0, 0.0,1.0,0.0 };
		alListenerfv(AL_ORIENTATION, listenerOri);
		dAssert(alGetError() == AL_NO_ERROR);
	
		/*
		// fmod coordinate system uses (0,0,1) as from vector
		m_coordinateSystem[0] = dVector (0.0f, 0.0f, 1.0f);
		m_coordinateSystem[1] = dVector (0.0f, 1.0f, 0.0f);
		m_coordinateSystem[2] = dVector (1.0f, 0.0f, 0.0f);
		*/
	}
}

ndSoundManager::~ndSoundManager()
{
	if (m_device)
	{
		m_assets.RemoveAll();
		alcDestroyContext(m_context);
		alcCloseDevice(m_device);
	}
}

void ndSoundManager::LoadWaveFile(ndSoundAsset* const asset, const char* const fileName)
{
	char path[2048];
	dGetWorkingFileName(fileName, path);

	FILE* const wave = fopen(path, "rb");
	if (wave)
	{
		char xbuffer[5];
		memset(xbuffer, 0, sizeof(xbuffer));
		fread(xbuffer, sizeof(char), 4, wave);
		if (!strcmp(xbuffer, "RIFF"))
		{
			dInt32 chunkSize;
			fread(&chunkSize, sizeof(dInt32), 1, wave);
			fread(xbuffer, sizeof(char), 4, wave);
			if (!strcmp(xbuffer, "WAVE"))
			{
				fread(xbuffer, sizeof(char), 4, wave);
				if (!strcmp(xbuffer, "fmt "))
				{
					fread(&chunkSize, sizeof(dInt32), 1, wave);

					dInt32 sampleRate;
					dInt32 byteRate;
					short channels;
					short audioFormat;
					short blockAlign;
					short bitsPerSample;

					fread(&audioFormat, sizeof(short), 1, wave);
					fread(&channels, sizeof(short), 1, wave);
					fread(&sampleRate, sizeof(dInt32), 1, wave);
					fread(&byteRate, sizeof(dInt32), 1, wave);
					fread(&blockAlign, sizeof(short), 1, wave);
					fread(&bitsPerSample, sizeof(short), 1, wave);
					for (dInt32 i = 0; i < (chunkSize - 16); i++)
					{
						fread(xbuffer, sizeof(char), 1, wave);
					}

					#define WAVE_FORMAT_PCM 0x0001
					//0x0003 WAVE_FORMAT_IEEE_FLOAT IEEE float 
					//0x0006 WAVE_FORMAT_ALAW 8-bit ITU-T G.711 A-law 
					//0x0007 WAVE_FORMAT_MULAW 8-bit ITU-T G.711 �-law 
					//0xFFFE WAVE_FORMAT_EXTENSIBLE Determined by SubFormat 

					// I only parse WAVE_FORMAT_PCM format
					dAssert(audioFormat == WAVE_FORMAT_PCM);

					fread(xbuffer, sizeof(char), 4, wave);
					if (!strcmp(xbuffer, "fact"))
					{
						dInt32 size;
						dInt32 samplesPerChannels;
						fread(&size, sizeof(dInt32), 1, wave);
						fread(&samplesPerChannels, sizeof(dInt32), 1, wave);
						fread(xbuffer, sizeof(char), 4, wave);
					}

					if (!strcmp(xbuffer, "data"))
					{
						dInt32 size;
						fread(&size, sizeof(dInt32), 1, wave);

						dArray<char> data;
						data.SetCount(size);
						fread(&data[0], sizeof(char), size, wave);

						dInt32 waveFormat = AL_FORMAT_MONO8;
						if (channels == 1)
						{
							if (bitsPerSample == 8)
							{
								waveFormat = AL_FORMAT_MONO8;
							}
							else
							{
								dAssert(bitsPerSample == 16);
								waveFormat = AL_FORMAT_MONO16;
							}
						}
						else
						{
							dAssert(channels == 2);
							if (bitsPerSample == 8)
							{
								waveFormat = AL_FORMAT_STEREO8;
							}
							else
							{
								dAssert(bitsPerSample == 16);
								waveFormat = AL_FORMAT_STEREO16;
							}
						}

						asset->m_lenght = dFloat32(size) / byteRate;
						asset->m_frequecy = dFloat32(sampleRate);
						alBufferData(asset->m_buffer, waveFormat, &data[0], size, sampleRate);
					}
				}
			}
		}
		fclose(wave);
	}
}
void ndSoundManager::PostUpdate(ndWorld* const, dFloat32)
{
	if (m_device)
	{
		ndSoundChannelPlaying::dNode* next;
		for (ndSoundChannelPlaying::dNode* node = m_channelPlaying.GetFirst(); node; node = next)
		{
			ndSoundChannel* const channel = node->GetInfo();
			next = node->GetNext();

			if (!channel->IsPlaying())
			{
				channel->Stop();
			}
		}
	}
}

ndSoundAsset* ndSoundManager::CreateSoundAsset(const char* const fileName)
{
	ndSoundAssetList::dNode* assetNode = nullptr;
	if (m_device)
	{
		dUnsigned64 code = dCRC64(fileName);
		assetNode = m_assets.Find(code);
		if (!assetNode)
		{
			assetNode = m_assets.Insert(code);
			LoadWaveFile(&assetNode->GetInfo(), fileName);
			assetNode->GetInfo().m_node = assetNode;
		}
	}
	return &assetNode->GetInfo();
}

ndSoundChannel* ndSoundManager::CreateSoundChannel(const char* const fileName)
{
	ndSoundChannel* channel = nullptr;
	if (m_device)
	{
		dUnsigned64 code = dCRC64(fileName);
		ndSoundAssetList::dNode* const assetNode = m_assets.Find(code);
		dAssert(assetNode);
	
		ndSoundAsset& asset = assetNode->GetInfo();
		ndSoundChannelList::dNode* const channelNode = asset.Append();
		channel = &channelNode->GetInfo();

		channel->m_asset = &asset;
		channel->m_manager = this;
		alSourcei(channel->m_source, AL_BUFFER, asset.m_buffer);
		dAssert(alGetError() == AL_NO_ERROR);
	}
	return channel;
}

