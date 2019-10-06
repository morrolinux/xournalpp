#include "VorbisProducer.h"


VorbisProducer::VorbisProducer(AudioQueue<float>* audioQueue) : audioQueue(audioQueue)
{
}

VorbisProducer::~VorbisProducer()
{
}

bool VorbisProducer::start(std::string filename, unsigned int timestamp)
{
	this->sfInfo.format = 0;
	this->sfFile = sf_open(filename.c_str(), SFM_READ, &sfInfo);
	if (sfFile == nullptr)
	{
		g_warning("VorbisProducer: input file \"%s\" could not be opened\ncaused by:%s", filename.c_str(), sf_strerror(sfFile));
		return false;
	}

	sf_count_t seekPosition = this->sfInfo.samplerate / 1000 * timestamp;
	this->startPosition = seekPosition;

	if (seekPosition < this->sfInfo.frames)
	{
		sf_seek(this->sfFile, seekPosition, SEEK_SET);
	}
	else
	{
		g_warning("VorbisProducer: Seeking outside of audio file extent");
	}

	this->audioQueue->setAudioAttributes(this->sfInfo.samplerate, static_cast<unsigned int>(this->sfInfo.channels));

	this->producerThread = new std::thread(
			[&, filename]
			{
				long numSamples = 1;
				auto sampleBuffer = new float[1024 * this->sfInfo.channels];
				long tot = this->startPosition;
				
				while (!this->stopProducer && numSamples > 0 && !this->audioQueue->hasStreamEnded())
				{
					numSamples = sf_readf_float(this->sfFile, sampleBuffer, 1024);
					tot+=numSamples;

					while (this->audioQueue->size() >= this->sample_buffer_size && !this->audioQueue->hasStreamEnded() && !this->stopProducer)
					{
						std::this_thread::sleep_for(std::chrono::microseconds(100));
					}

					if (this->seekSeconds != 0)
					{
						tot += seekSeconds * this->sfInfo.samplerate;
						sf_seek(this->sfFile, tot, SEEK_SET);
						this->seekSeconds = 0;
					}

					this->audioQueue->push(sampleBuffer, static_cast<unsigned long>(numSamples * this->sfInfo.channels));
				}
				this->audioQueue->signalEndOfStream();

				delete[] sampleBuffer;
				sampleBuffer = nullptr;
				this->startPosition = 0;

				sf_close(this->sfFile);
			});
	return true;
}

void VorbisProducer::abort()
{
	this->stopProducer = true;
	// Wait for producer to finish
	stop();
	this->stopProducer = false;

}

void VorbisProducer::stop()
{
	// Wait for producer to finish
	if (this->producerThread && this->producerThread->joinable())
	{
		this->producerThread->join();
	}
}


void VorbisProducer::seek(int seconds)
{
	XOJ_CHECK_TYPE(VorbisProducer);

	this->seekSeconds = seconds;
}