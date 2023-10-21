/*
 * This software is provided as is, without any warranty, express or implied.
 * Copyright 2023 Chris Malnick. All rights reserved.
 */


#include <libftd3xx/ftd3xx.h>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <thread>
#include <unistd.h> 
#include <iostream>
#include <queue>
#include <vector>

#define WINDOWS 1

#define NAME "xx3dsfml"
#define TNAME "xx3dsfml - top"
#define BNAME "xx3dsfml - bottom"
#define NUM_PRODUCTS 2
#define PRODUCTS (const char*[]){"N3DSXL", "N3DSXL.2"}

#define BULK_OUT 0x02
#define BULK_IN 0x82
#define TBULK_IN 0x86

#define FIFO_CHANNEL 0

#define CAP_WIDTH 240
#define CAP_HEIGHT 720

#define CAP_RES (CAP_WIDTH * CAP_HEIGHT)

#define RGB_FRAME_SIZE (CAP_RES * 3)
#define RGBA_FRAME_SIZE (CAP_RES * 4)

#define EXTRA_BUF_SIZE 7168
#define AUDIO_CHANNELS 2
#define AUDIO_SAMPLE_RATE 32728

#define BUF_COUNT 8
#define BUF_SIZE (RGB_FRAME_SIZE + EXTRA_BUF_SIZE)

#define TOP_WIDTH 400
#define TOP_HEIGHT 240

#define TOP_RES (TOP_WIDTH * TOP_HEIGHT)

#define BOT_WIDTH 320
#define BOT_HEIGHT 240

#define DELTA_WIDTH (TOP_WIDTH - BOT_WIDTH)
#define DELTA_RES (DELTA_WIDTH * TOP_HEIGHT)

#define FRAMERATE 60

FT_HANDLE handle;

ULONG device_id;
char * device_description = new char[32];
char * device_serial_number = new char[16];

uint8_t connected_3ds = 0;
bool connected = false;
bool running = true;
bool disconnect_and_connect = false;

int curr_buf = 0;
UCHAR in_buf[BUF_COUNT][BUF_SIZE];
ULONG len_buf[BUF_COUNT];

typedef std::vector<sf::Int16> IntVector;
class N3DSAudio : public sf::SoundStream
{
public:
	N3DSAudio(){
		initialize(AUDIO_CHANNELS, AUDIO_SAMPLE_RATE);
		setProcessingInterval(sf::milliseconds(0));
		setVolume(20);
	} 

	void queue(IntVector samples){
		m_bmux.lock();
		if(m_buff.size() <= BUF_COUNT * 10){
			m_buff.push(samples);
		}
		m_bmux.unlock();
	}
private:
	sf::Mutex m_bmux;
	std::queue<IntVector> m_buff;
	size_t m_sampleRate;

	virtual bool onGetData(Chunk& data){
		m_bmux.lock();
		if(m_buff.size()<1)
		{
			m_bmux.unlock();
			return true;
		}

		IntVector bu = m_buff.front();
		m_buff.pop();

		m_bmux.unlock();

		size_t bfs=bu.size();
		sf::Int16* bf=new sf::Int16[bfs]; 

		for(size_t i=0;i<bfs;++i)
			bf[i]=bu[i];

		
		data.samples   = bf;
		data.sampleCount = bfs;

		return true;
	}

	virtual void onSeek(sf::Time timeOffset){
		// no op
	}
};

bool handle_open(int8_t idx){
	if (FT_SUCCESS(FT_Create(device_description, FT_OPEN_BY_DESCRIPTION, &handle))) {
		printf("[%s] Create succeeded.\n", NAME);
		return true;
	}
	return false;
};

int32_t ask_for_audio(){
	UCHAR buf[4] = {0x40, 0x80, 0x00, 0x00};
	ULONG written = 0;

	if (FT_FAILED(FT_WritePipe(handle, BULK_OUT, buf, 4, &written, 0))) {
		FT_AbortPipe(handle, BULK_OUT);
		printf("[%s] Write 1 failed.\n", NAME);
		return 0;
	}
	
	if (FT_FAILED(FT_AbortPipe(handle, BULK_IN))) {
		printf("[%s] Abort failed.\n", NAME);
		return 0;
	}
	
	UCHAR buf2[16] = {0x98, 0x05, 0x9f, 0x0};
	ULONG returned = 0;
	
	if (FT_FAILED(FT_WritePipe(handle, BULK_OUT, buf2, 4, &returned, 0))) {
		FT_AbortPipe(handle, BULK_OUT);
		printf("[%s] Write 2 failed.\n", NAME);
		return 0;
	}

	if (FT_FAILED(FT_ReadPipe(handle, BULK_IN, buf2, 16, &returned, 0))) {
		FT_AbortPipe(handle, BULK_IN);
		printf("[%s] Read failed.\n", NAME);
		return 0;
	}
	
	uint32_t bsId = (((((buf2[4] << 8) | buf2[3]) << 8) | buf2[2]) << 8) | buf2[1];
	return (bsId & 0xf0f0ff) == 0xc0b0a1 ? bsId : 0;
}

bool initialize(){
	UCHAR buf[4] = {0x40, 0x00, 0x00, 0x00};
	ULONG written = 0;

	if (FT_FAILED(FT_WritePipe(handle, BULK_OUT, buf, 4, &written, 0))) {
		FT_AbortPipe(handle, BULK_OUT);
		printf("[%s] Write 7 failed.\n", NAME);
		return false;
	}

	if (FT_FAILED(FT_SetStreamPipe(handle, false, false, BULK_IN, BUF_SIZE))) {
		printf("[%s] Stream failed.\n", NAME);
		return false;
	}

	return true;
}

int8_t* listDevices(){
	static int8_t info[3];
	info[0] = 0;
	FT_STATUS ftStatus;
	DWORD numDevs = 0;
	ftStatus = FT_CreateDeviceInfoList(&numDevs);
	if (FT_SUCCESS(ftStatus)){
		if(numDevs < connected_3ds){
			disconnect_and_connect = false;
		}
		connected_3ds = numDevs;

		if(numDevs > 0){
			FT_DEVICE_LIST_INFO_NODE *devInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(
			sizeof(FT_DEVICE_LIST_INFO_NODE) * numDevs);
			ftStatus = FT_GetDeviceInfoList(devInfo, &numDevs);
			if (FT_SUCCESS(ftStatus))
			{
				for (DWORD i = 0; i < numDevs; i++)
				{
					for (int8_t j=0; j < NUM_PRODUCTS; j++) {
						const char* p = PRODUCTS[j];
						if(strcmp(devInfo[i].Description, p) == 0){
							info[0] = j+1; // Index on PRODUCTS + 1 of existance
							info[1] = devInfo[i].Flags & FT_FLAGS_OPENED; // ISOPEN
							info[2] = (devInfo[i].Flags & FT_FLAGS_SUPERSPEED) ? 3 : (devInfo[i].Flags & FT_FLAGS_HISPEED) ? 2 : 0; // USBVERSION
							handle = devInfo[i].ftHandle; // HANDLE
							memcpy (device_description, devInfo[i].Description, 32);
							memcpy (device_serial_number, devInfo[i].SerialNumber, 16);
							device_id = devInfo[i].ID;
							break;
						}
					}

					if(info[0]){
						break;
					}
				}
			}
			free(devInfo);
		}
	}
	
	return info;
}

uint32_t getFW(){
	UCHAR buf[4] = {0x80, 0x01, 0xab, 0x00};
	ULONG written = 0;

	if (FT_FAILED(FT_WritePipe(handle, BULK_OUT, buf, 4, &written, 0))) {
		FT_AbortPipe(handle, BULK_OUT);
		printf("[%s] Write 3 failed.\n", NAME);
		return -1;
	}
	
	UCHAR buf2[8] = {0x90, 0x08, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00};
	ULONG returned = 0;
	
	if (FT_FAILED(FT_WritePipe(handle, BULK_OUT, buf2, 8, &returned, 0))) {
		FT_AbortPipe(handle, BULK_OUT);
		printf("[%s] Write 4 failed.\n", NAME);
		return -1;
	}
	
	if (FT_FAILED(FT_ReadPipe(handle, BULK_IN, buf2, 8, &returned, 0))) {
		FT_AbortPipe(handle, BULK_IN);
		printf("[%s] Read failed.\n", NAME);
		return -1;
	}

	uint32_t fwId = (((((buf2[7] << 8) | buf2[6]) << 8) | buf2[5]) << 8) | buf2[4];
	return (fwId & 0xf0feff) == 0xc0b0a1 ? (fwId >> 24) : 0;
}

uint32_t reset(){
	UCHAR buf[4] = {0x80, 0x01, 0xab, 0x00};
	ULONG written = 0;
	
	if (FT_FAILED(FT_WritePipe(handle, BULK_OUT, buf, 4, &written, 0))) {
		FT_AbortPipe(handle, BULK_OUT);
		printf("[%s] Write 5 failed.\n", NAME);
		return 0;
	}
	
	UCHAR buf2[4] = {0x43, 0x00, 0x00, 0x00};
	ULONG returned = 0;
	
	if (FT_FAILED(FT_WritePipe(handle, BULK_OUT, buf2, 4, &returned, 0))) {
		FT_AbortPipe(handle, BULK_OUT);
		printf("[%s] Write 6 failed.\n", NAME);
		return 0;
	}
	
	size_t i = 0;
	int32_t bsId;
	do
	{
		usleep(200);
		bsId = ask_for_audio();
		if (bsId != 0)
        {
			return (bsId & 0xF00) == 0x100 ? bsId : 0;
		}
		i++;
	} while (i < 3);
	return bsId;
}

bool close() {
	disconnect_and_connect = true;

	if (FT_Close(handle)) {
		printf("[%s] Close failed.\n", NAME);
		return false;
	}

	printf("[%s] Connection closed.\n", NAME);
	connected = false;
	return true;
}

bool open() {
	if (connected || disconnect_and_connect) {
		return false;
	}

	int8_t *info = listDevices();
	if(!info[0]){
		return false;
	}

	if (!handle_open(info[1])) {
		printf("[%s] Create failed.\n", NAME);
		disconnect_and_connect = true;
		return false;
	}

	if(FT_SetPipeTimeout(handle, BULK_OUT, 200)){
		printf("[%s] Timeout failed.\n", NAME);
		close();
		return false;
	}

	if(FT_SetPipeTimeout(handle, BULK_IN, 80)){
		printf("[%s] Timeout failed.\n", NAME);
		close();
		return false;
	}

	if(FT_ClearStreamPipe(handle, true, true, 0)){
		printf("[%s] Clear failed.\n", NAME);
		close();
		return false;
	}

	int32_t bsId = ask_for_audio();
	if (!bsId) {
		printf("[%s] Ask for audio failed.\n", NAME);
		close();
		return false;
	}

	int32_t parse = (bsId & 0xf00);
	uint32_t fw;
	if(parse != 0x0){
		fw = (bsId >> 24);
	}else{
		fw = getFW();
	}

	if(parse == 0x0 && fw < 0){
		printf("[%s] Get FW failed.\n", NAME);
		close();
		return false;
	}

	printf("[%s] %s, SN=%s, FW=%d, USB%d\n", NAME, device_description, device_serial_number, fw, info[2]);

	if(parse == 0x0){
		bsId = reset();
		parse = (bsId & 0xf00);
		fw = -1;
		if(parse != 0x0){
			fw = (bsId >> 24);
		}
	}
	if(!bsId || fw < 0){
		printf("[%s] Get FW after reset failed.\n", NAME);
		close();
		return false;
	}

	if(!initialize()){
		printf("[%s] Initialize failed.\n", NAME);
		close();
		return false;
	}

	return true;
}

void capture() {
	ULONG read[BUF_COUNT];
	OVERLAPPED overlap[BUF_COUNT];

start:
	uint64_t i = 0;
	while (!connected) {
		if (!running) {
			return;
		}

		int8_t *info = listDevices();
		if(i){
			printf("\x1b[A");
		}
		i++;
		if(disconnect_and_connect){
			printf("[%s] please disconnect and connect cable.\n", NAME);
		}else{
			printf("[%s] please connect 3ds.                 \n", NAME);

			if(info[0]){
				connected = open();
			}
		}

		sleep(1);
	}
	
	for (curr_buf = 0; curr_buf < BUF_COUNT; ++curr_buf) {
		if (FT_InitializeOverlapped(handle, &overlap[curr_buf])) {
			printf("[%s] Initialize failed.\n", NAME);
			goto end;
		}
	}
	
	for (curr_buf = 0; curr_buf < BUF_COUNT; ++curr_buf) {
		if (FT_ReadPipeAsync(handle, FIFO_CHANNEL, in_buf[curr_buf], BUF_SIZE, &read[curr_buf], &overlap[curr_buf]) != FT_IO_PENDING) {
			printf("[%s] Read failed.\n", NAME);
			goto end;
		}
	}

	curr_buf = 0;

	while (connected && running) {
		if (FT_GetOverlappedResult(handle, &overlap[curr_buf], &read[curr_buf], true) == FT_IO_INCOMPLETE && FT_AbortPipe(handle, BULK_IN)) {
			printf("[%s] Abort failed.\n", NAME);
			goto end;
		}

		len_buf[curr_buf] = read[curr_buf];

		if (FT_ReadPipeAsync(handle, FIFO_CHANNEL, in_buf[curr_buf], BUF_SIZE, &read[curr_buf], &overlap[curr_buf]) != FT_IO_PENDING) {
			printf("[%s] Read failed.\n", NAME);
			goto end;
		}

		if (++curr_buf == BUF_COUNT) {
			curr_buf = 0;
		}
	}

end:
	for (curr_buf = 0; curr_buf < BUF_COUNT; ++curr_buf) {
		if (FT_FAILED(FT_ReleaseOverlapped(handle, &overlap[curr_buf]))) {
			printf("[%s] Release failed.\n", NAME);
		}
	}

	FT_AbortPipe(handle, BULK_OUT);

	if (FT_FAILED(FT_Close(handle))) {
		printf("[%s] Close failed.\n", NAME);
	}

	printf("[%s] Connection closed.\n", NAME);
	connected = false;

	goto start;
}

void map(UCHAR *p_in, UCHAR *p_out) {
	for (int i = 0, j = DELTA_RES, k = TOP_RES; i < CAP_RES; ++i) {
		if (i < DELTA_RES) {
			p_out[4 * i + 0] = p_in[3 * i + 0];
			p_out[4 * i + 1] = p_in[3 * i + 1];
			p_out[4 * i + 2] = p_in[3 * i + 2];
			p_out[4 * i + 3] = 0xff;
		}

		else if (i / CAP_WIDTH & 1) {
			p_out[4 * j + 0] = p_in[3 * i + 0];
			p_out[4 * j + 1] = p_in[3 * i + 1];
			p_out[4 * j + 2] = p_in[3 * i + 2];
			p_out[4 * j + 3] = 0xff;

			++j;
		}

		else {
			p_out[4 * k + 0] = p_in[3 * i + 0];
			p_out[4 * k + 1] = p_in[3 * i + 1];
			p_out[4 * k + 2] = p_in[3 * i + 2];
			p_out[4 * k + 3] = 0xff;

			++k;
		}
	}
}

void audio(UCHAR *p_in, ULONG end, N3DSAudio *soundStream) {
	IntVector sample;
	size_t seed = RGB_FRAME_SIZE - end;
	for (size_t i=RGB_FRAME_SIZE;i<end; i=i+2) {
		sf::Int16 sound = (p_in[i+1] << 8) | p_in[i];
		sample.push_back(sound);
	}
	
	if(sample.size()> 0){
		soundStream->queue(sample);
		
		if(soundStream->getStatus() != sf::SoundSource::Playing)
			soundStream->play();
	}
}

void render() {
	std::thread thread(capture);

	int win_width, win_height, wint_width, wint_height, winb_width, winb_height;

	int scale = 1;
	int last_idx = -1;

	int windows = WINDOWS;

	const char * name = NAME;

	UCHAR out_buf[RGBA_FRAME_SIZE];
	sf::RenderWindow* win[2];

	sf::RectangleShape top_rect(sf::Vector2f(wint_height, wint_width));
	sf::RectangleShape bot_rect(sf::Vector2f(winb_height, winb_width));

	sf::RectangleShape out_rect(sf::Vector2f(win_width, win_height));

	sf::Texture in_tex;
	sf::RenderTexture out_tex;

	sf::Event event;	

	N3DSAudio *audioStream = new N3DSAudio();

change:
	win_width = TOP_WIDTH;
	win_height = TOP_HEIGHT + BOT_HEIGHT;

	wint_width = TOP_WIDTH;
	wint_height = TOP_HEIGHT;

	winb_width = BOT_WIDTH;
	winb_height = BOT_HEIGHT;

	top_rect.setRotation(0);
	bot_rect.setRotation(0);
	out_rect.setRotation(0);

	if(windows == 2){
		win[0] = new sf::RenderWindow(sf::VideoMode(wint_width, wint_height), TNAME);
		win[1] = new sf::RenderWindow(sf::VideoMode(winb_width, winb_height), BNAME);
	}else{
		win[0] = new sf::RenderWindow(sf::VideoMode(win_width, win_height), NAME);
	}

	win[0]->setFramerateLimit(FRAMERATE + FRAMERATE / 2);
	win[0]->setKeyRepeatEnabled(false);
	
	if(windows == 2){
		win[1]->setFramerateLimit(FRAMERATE + FRAMERATE / 2);
		win[1]->setKeyRepeatEnabled(false);

		sf::View viewt(sf::FloatRect(0, 0, wint_width, wint_height));
		win[0]->setView(viewt);

		sf::View viewb(sf::FloatRect(0, 0, winb_width, winb_height));
		win[1]->setView(viewb);
		
		win[0]->setSize(sf::Vector2u(wint_width * scale, wint_height * scale));
		win[1]->setSize(sf::Vector2u(winb_width * scale, winb_height * scale));
	}else{
		sf::View view(sf::FloatRect(0, 0, win_width, win_height));
		win[0]->setView(view);
		win[0]->setSize(sf::Vector2u(win_width * scale, win_height * scale));
	}

	top_rect.setSize(sf::Vector2f(wint_height, wint_width));
	top_rect.setOrigin(wint_height / 2, wint_width / 2);
	top_rect.setPosition(wint_width / 2, wint_height / 2);
	top_rect.setRotation(-90);

	bot_rect.setSize(sf::Vector2f(winb_height, winb_width));
	bot_rect.setOrigin(winb_height / 2, winb_width / 2);
	if(windows == 2){
		bot_rect.setPosition(winb_width / 2, winb_height / 2);
	}else{
		bot_rect.setPosition(win_width / 2, TOP_HEIGHT + BOT_HEIGHT - (winb_height / 2));
	}
	bot_rect.setRotation(-90);

	out_rect.setSize(sf::Vector2f(win_width, win_height));
	out_rect.setOrigin(win_width / 2, win_height / 2);
	out_rect.setPosition(win_width / 2, win_height / 2);

	in_tex.create(CAP_WIDTH, CAP_HEIGHT);
	out_tex.create(win_width, win_height);

	top_rect.setTexture(&in_tex);
	top_rect.setTextureRect(sf::IntRect(0, 0, TOP_HEIGHT, TOP_WIDTH));

	bot_rect.setTexture(&in_tex);
	bot_rect.setTextureRect(sf::IntRect(0, TOP_WIDTH, BOT_HEIGHT, BOT_WIDTH));

	out_rect.setTexture(&out_tex.getTexture());

	while (win[0]->isOpen()) {
		try {
			while (win[0]->pollEvent(event)) {
				switch (event.type) {
				case sf::Event::Closed:
					win[0]->close();
					if(windows == 2){
						win[1]->close();
					}
					break;

				case sf::Event::KeyPressed:
					switch (event.key.code) {
					case sf::Keyboard::Num1:
						//connected = open();
						break;

					case sf::Keyboard::Num2:
						out_tex.setSmooth(!out_tex.isSmooth());
						break;

					case sf::Keyboard::Num3:
						scale -= scale == 1 ? 0 : 1;
						if(windows == 2){
							win[0]->setSize(sf::Vector2u(wint_width * scale, wint_height * scale));
							win[1]->setSize(sf::Vector2u(winb_width * scale, winb_height * scale));
						}else{
							win[0]->setSize(sf::Vector2u(win_width * scale, win_height * scale));
						}
						break;

					case sf::Keyboard::Num4:
						scale += scale == 4 ? 0 : 1;
						if(windows == 2){
							win[0]->setSize(sf::Vector2u(wint_width * scale, wint_height * scale));
							win[1]->setSize(sf::Vector2u(winb_width * scale, winb_height * scale));
						}else{
							win[0]->setSize(sf::Vector2u(win_width * scale, win_height * scale));
						}
						break;

					case sf::Keyboard::Num5:
						if(windows == 2){
							std::swap(wint_width, wint_height);
							std::swap(winb_width, winb_height);

							top_rect.setSize(sf::Vector2f(wint_height, wint_width));
							top_rect.setOrigin(wint_height / 2, wint_width / 2);
							top_rect.rotate(-90);

							bot_rect.setSize(sf::Vector2f(winb_height, winb_width));
							bot_rect.setOrigin(winb_height / 2, winb_width / 2);
							bot_rect.rotate(-90);

							win[0]->setSize(sf::Vector2u(wint_width * scale, wint_height * scale));
							win[1]->setSize(sf::Vector2u(winb_width * scale, winb_height * scale));
						}else{
							std::swap(win_width, win_height);

							out_rect.setSize(sf::Vector2f(win_width, win_height));
							out_rect.setOrigin(win_width / 2, win_height / 2);
							out_rect.rotate(-90);

							win[0]->setSize(sf::Vector2u(win_width * scale, win_height * scale));
						}

						break;

					case sf::Keyboard::Num6:
						if(windows == 2){
							std::swap(wint_width, wint_height);
							std::swap(winb_width, winb_height);

							top_rect.setSize(sf::Vector2f(wint_height, wint_width));
							top_rect.setOrigin(wint_height / 2, wint_width / 2);
							top_rect.rotate(90);

							bot_rect.setSize(sf::Vector2f(winb_height, winb_width));
							bot_rect.setOrigin(winb_height / 2, winb_width / 2);
							bot_rect.rotate(90);

							win[0]->setSize(sf::Vector2u(wint_width * scale, wint_height * scale));
							win[1]->setSize(sf::Vector2u(winb_width * scale, winb_height * scale));
						}else{
							std::swap(win_width, win_height);

							out_rect.setSize(sf::Vector2f(win_width, win_height));
							out_rect.setOrigin(win_width / 2, win_height / 2);
							out_rect.rotate(90);

							win[0]->setSize(sf::Vector2u(win_width * scale, win_height * scale));
						}

						break;

					case sf::Keyboard::Num0:
						win[0]->close();
						delete win[0];
						if(windows == 2){
							win[1]->close();
							delete win[1];
						}
						windows = (windows == 2 ? 1 : 2);
						goto change;
						break;

					default:
						break;
					}

					break;

				default:
					break;
				}
			}

			win[0]->clear();
			if(windows == 2){
				win[1]->clear();
			}

			if (connected) {
				int idx = (curr_buf == 0 ? BUF_COUNT : curr_buf) - 1;
				if(last_idx != idx){
					map(in_buf[idx], out_buf);
					audio(in_buf[idx], len_buf[idx], audioStream);
					last_idx = idx;
				}

				in_tex.update(out_buf, CAP_WIDTH, CAP_HEIGHT, 0, 0);

				out_tex.clear();

				out_tex.draw(top_rect);
				out_tex.draw(bot_rect);

				out_tex.display();

				if(windows == 2){
					win[0]->draw(top_rect);
					win[1]->draw(bot_rect);
				}else{
					win[0]->draw(out_rect);
				}
			}

			win[0]->display();
			if(windows == 2){
				win[1]->display();
			}
		}catch(...){
			printf("[%s] Exception.\n", NAME);
		}
	}
	
	if(windows == 2){
		win[1]->close();
		delete win[1];
	}
	delete win[0];

	audioStream->stop();
	delete audioStream;

	running = false;
	thread.join();
}

int main() {
	render();
	return 0;
}
