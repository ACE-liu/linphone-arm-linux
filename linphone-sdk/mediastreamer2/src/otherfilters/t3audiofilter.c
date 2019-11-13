
#if defined(HAVE_CONFIG_H)
#include "mediastreamer-config.h"
#endif



#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/mssndcard.h"

#include "faad.h"
#include<sys/socket.h>
#include<netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>


#define KEEP_COONECT_TO_T3AUDIO

static int forced_rate=-1;

static char* audio_read_ip = "192.168.43.1";
static int audio_read_port = 3708;

static char* audio_write_ip = "192.168.1.3";
static int audio_write_port = 8081;

static char * fileName = "record_recv.wav";
FILE *fp = NULL;

static char * sendfileName = "record_send.wav";
FILE *sendfp = NULL;

typedef struct _riff_t1 {
char riff[4]; /* “RIFF” (ASCII characters) */
uint32_t len; /* Length of package (binary, little endian) */
char wave[4]; /* “WAVE” (ASCII characters) */
} riff_t1;

/* The FORMAT chunk */

typedef struct _format_t1 {
char fmt[4]; /* “fmt_” (ASCII characters) */
uint32_t len; /* length of FORMAT chunk (always 0x10) */
uint16_t type; /* codec type*/
uint16_t channel; /* Channel numbers (0x01 = mono, 0x02 = stereo) */
uint32_t rate; /* Sample rate (binary, in Hz) */
uint32_t bps; /* Average Bytes Per Second */
uint16_t blockalign; 
uint16_t bitpspl; /* bits per sample */
} format_t1;

typedef struct _data_t1 {
char data[4]; /* “data” (ASCII characters) */
uint32_t len; /* length of data */
}data_t1;

typedef struct _wav_head1
{
riff_t1 riff;
format_t1 format;
data_t1 data;
}wav_head1;

static wav_head1 header;
static bool_t firstWriteFile =TRUE;

static bool_t aac_init = FALSE;

static unsigned long sample_rate = 48000;
static unsigned char channelNum = 1;
static   NeAACDecHandle decoder = 0;

struct _T3AudioAACData{
	char data[1024];
	int dataLen;
};
static void t3audio_card_detect(MSSndCardManager *m);
typedef struct _T3AudioAACData T3AudioAACData;

void ms_t3audio_card_set_forced_sample_rate(int samplerate){
	if (samplerate==0 || samplerate<-1) {
		ms_warning("ms_t3audio_card_set_forced_sample_rate(): bad value %i",samplerate);
		return;
	}
	forced_rate=samplerate;
}

//#define THREADED_VERSION

/*in case of troubles with a particular driver, try incrementing 
to 512, 1024, 2048, 4096...
then try incrementing the number of periods*/
#define T3AUDIO_PERIODS 8
#define T3AUDIO_PERIOD_SIZE 256


static MSSndCard *t3audio_card_duplicate(MSSndCard *obj);
static MSFilter * ms_t3audio_read_new(MSFactory *factory, const char *dev);
static MSFilter * ms_t3audio_write_new(MSFactory *factory, const char *dev);


struct _T3AudioData{
	char *pcmdev;
	char *mixdev;
};

typedef struct _T3AudioData T3AudioData;



typedef enum {CAPTURE, PLAYBACK, CAPTURE_SWITCH, PLAYBACK_SWITCH} t3AudioMixerAction;



static void t3audio_card_set_level(MSSndCard *obj,MSSndCardMixerElem e,int a)
{
}

static int t3audio_card_get_level(MSSndCard *obj, MSSndCardMixerElem e)
{

	int value = -1;
	return value;
}

static void t3audio_card_set_source(MSSndCard *obj,MSSndCardCapture source)
{
	// T3AudioData *ad=(T3AudioData*)obj->data;
}

static MSFilter *t3audio_card_create_reader(MSSndCard *card)
{
	T3AudioData *ad=(T3AudioData*)card->data;
	MSFilter *f=ms_t3audio_read_new(ms_snd_card_get_factory(card), ad->pcmdev);
	return f;
}

static MSFilter *t3audio_card_create_writer(MSSndCard *card)
{
	T3AudioData *ad=(T3AudioData*)card->data;
	MSFilter *f=ms_t3audio_write_new(ms_snd_card_get_factory(card), ad->pcmdev);
	return f;
}



static void t3audio_card_init(MSSndCard *obj){
	T3AudioData *ad=ms_new0(T3AudioData,1);
	obj->data=ad;


}

static void t3audio_card_uninit(MSSndCard *obj){
	T3AudioData *ad=(T3AudioData*)obj->data;
	if (ad->pcmdev!=NULL) ms_free(ad->pcmdev);
	if (ad->mixdev!=NULL) ms_free(ad->mixdev);
	ms_free(ad);
}


MSSndCardDesc t3audio_card_desc={
	.driver_type="t3audio",
	.detect=t3audio_card_detect,
	.init=t3audio_card_init,
	.set_level=t3audio_card_set_level,
	.get_level=t3audio_card_get_level,
	.set_capture=t3audio_card_set_source,
	.set_control=NULL,
	.get_control=NULL,
	.create_reader=t3audio_card_create_reader,
	.create_writer=t3audio_card_create_writer,
	.uninit=t3audio_card_uninit,
	.duplicate=t3audio_card_duplicate
};

static unsigned int get_t3audio_card_capabilities(){
	unsigned int ret = 0;
    bool_t canRead = 1, canWrite = 1;
	if (canRead) {
		ret|=MS_SND_CARD_CAP_CAPTURE;
	}
	if (canWrite) {
		ret|=MS_SND_CARD_CAP_PLAYBACK;
	}
	return ret;
}

static void t3audio_card_detect(MSSndCardManager *m){
	MSSndCard *card=NULL;
	
	card=ms_snd_card_new(&t3audio_card_desc);
	
	card->capabilities =get_t3audio_card_capabilities();
	if (card) 
	{
		T3AudioData* ad=(T3AudioData*)card->data;
	    card->name = ms_strdup("t3audio");
		ad->pcmdev=ms_strdup("default");
		ad->mixdev=ms_strdup("default");
		ms_snd_card_manager_add_card(m,card);
	}
}

static MSSndCard *t3audio_card_duplicate(MSSndCard *obj){
	MSSndCard *card=ms_snd_card_new(&t3audio_card_desc);
	T3AudioData* dcard=(T3AudioData*)card->data;
	T3AudioData* dobj=(T3AudioData*)obj->data;
	card->name=ms_strdup(obj->name);
	card->id=ms_strdup(obj->id);
	dcard->pcmdev=ms_strdup(dobj->pcmdev);
	dcard->mixdev=ms_strdup(dobj->mixdev);
	return card;
}

MSSndCard * ms_t3audio_card_new_custom(const char *pcmdev, const char *mixdev){
	MSSndCard * obj;
	T3AudioData *ad;
	obj=ms_snd_card_new(&t3audio_card_desc);
	ad=(T3AudioData*)obj->data;
	obj->name=ms_strdup(pcmdev);
	ad->pcmdev=ms_strdup(pcmdev);
	ad->mixdev=ms_strdup(mixdev);
	return obj;
}


struct _T3audioReadData{
	int rate;
	int nchannels;
	uint64_t read_samples;
	MSTickerSynchronizer *ticker_synchronizer;
	bool_t read_started;
	bool_t write_started;
    bool_t has_client_connect;
    bool_t run_thread_quit;
	bool_t start_read_data;
	ms_thread_t thread;
	ms_mutex_t mutex;
	int listenfd;
	int connfd;
	MSBufferizer * bufferizer;
};

typedef struct _T3audioReadData T3audioReadData;

static void faad_init()
{
    decoder = NeAACDecOpen();    
 
}

static void faad_uninit()
{
    NeAACDecClose(decoder);
	aac_init = FALSE;
}

static void aac_decode_data(char * input, const int len, char * output, int* outLen)
{
	NeAACDecConfigurationPtr conf;
	 NeAACDecFrameInfo frame_info;
	 char* pcm_data;
    if(!aac_init)
    {
        faad_init();
        conf = NeAACDecGetCurrentConfiguration(decoder);
        if(!conf){
            ms_warning("NeAACDecGetCurrentConfiguration failed\n");
            return;
        }
        conf->defSampleRate = sample_rate;
        conf->outputFormat = FAAD_FMT_16BIT;
        conf->dontUpSampleImplicitSBR = 1;
        NeAACDecSetConfiguration(decoder, conf);
         NeAACDecInit(decoder, (unsigned char*)input, len, &sample_rate, &channelNum);
         aac_init = TRUE;
    }
    pcm_data = (char*)NeAACDecDecode(decoder, &frame_info, (unsigned char *)input, len); 

    if(pcm_data && frame_info.samples > 0)
    {

        int samLen = frame_info.samples *2;
		if(frame_info.channels == 2)
		{
			for (int i = 0, j = 0; i<samLen && j<samLen/2; i += 4, j += 2)
			{
				output[j] = pcm_data[i];
				output[j + 1] = pcm_data[i + 1];
			}
			*outLen = samLen/2;
		}
		else
		{
			*outLen = samLen;
			memcpy(output, pcm_data, *outLen);
		}          

    }
	else if(frame_info.error > 0)
    {
        ms_warning("aac decode error=========%s\n",NeAACDecGetErrorMessage(frame_info.error));            
    }
}

// 每一帧视频或音频帧，增加了前8个字节头部，格式为: 0xa + 0xb + 0xc + droplvl + 4个字节长度
// droplvl：0是I帧，1是P帧，2是B帧
static void dealWithAAcaudioData(char *data, int len, char* outdata, int *outLen)
{
    if(len<=8)
    {     
      return;
    }
    if(data[0] == 0xa && data[1] == 0xb && data[2] == 0xc)
    {
        int len = (data[4]<<24)+(data[5]<<16)+ (data[6]<<8)+data[7]; 
        aac_decode_data(data+8, len, outdata, outLen);
    }
    else
    {
        ms_warning("t3audiofilter error format:: ");
    }
}

static unsigned int sendtotalLen = 0;
static wav_head1 sendheader;
static bool_t sendfirstWriteFile =TRUE;
static void initsendHeader()
{
	 int bits = 16;
	ms_warning("at initHeader....." );
    memcpy(sendheader.riff.riff, "RIFF",4);
    memcpy(sendheader.riff.wave, "WAVE",4);
    memcpy(sendheader.format.fmt, "fmt ", 4);
    sendheader.format.len = 0x10;
    sendheader.format.type = 0x1;
    sendheader.format.channel = 1;
    sendheader.format.rate = 48000;
    sendheader.format.bps = 48000 * bits/8 * 1;
    sendheader.format.blockalign = bits/8 * 1;
    sendheader.format.bitpspl = bits;

    memcpy(sendheader.data.data, "data", 4);
	ms_warning("out initHeader sendheader....." );
}
static void updatesendHeader()
{
    sendheader.data.len = (uint32_t)sendtotalLen;
	sendheader.riff.len = sendheader.data.len + 36;
}
static void beforesendCloseFile()
{
	ms_warning("at before beforesendCloseFile..............");
	if(!sendfirstWriteFile)
	{
    updatesendHeader();
    fseek(sendfp, 0, SEEK_SET);
    fwrite(&sendheader, sizeof(sendheader),1, sendfp);
    fclose(sendfp);
	sendfp = NULL;
	sendfirstWriteFile = TRUE;
	}
}


static void save_send_WavFile(char * data, const int len)
{
    if(sendfirstWriteFile)
    {
        if ((sendfp = fopen(sendfileName, "wb")) == NULL) 
	    {
            printf("create wav file error\n");
            return;
	    }
        initsendHeader();
        fwrite(&sendheader, sizeof(sendheader),1, sendfp);
		fflush(sendfp);
        sendfirstWriteFile = FALSE;
    }
    fwrite(data, len,1, sendfp);
	fflush(sendfp);
    sendtotalLen += len;
}

static void* t3audio_read_run(void *p)
{
	fd_set readfds; 
	int maxFd;
	int fdList[10] ={0};
	int fdSize = 10;
	char databuf[2048] ={0};
	char decodebuf[8192] ={0};
	int decodeLen =0;
	int m_nRecvLen;
	int result;
	int clientCount=0;
	mblk_t *om=NULL;
	struct timeval tv;
	T3audioReadData *ad = (T3audioReadData*)p;
    struct sockaddr_in  servaddr;
	 ms_warning("at t3audio_read_run............\n");
    if( (ad->listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        ms_warning("create socket error: %s(errno: %d)\n", strerror(errno),errno);
        return NULL;
    }
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(audio_read_port);
    servaddr.sin_addr.s_addr=inet_addr(audio_read_ip);
    if( bind(ad->listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
        ms_warning("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        return NULL;
    }
    if( listen(ad->listenfd, 10) == -1){
        ms_warning("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        return NULL;
    }

	 ms_warning("t3audio_read_run..listenfd: %d\n", ad->listenfd);
    while(!ad->run_thread_quit)
	{
		tv.tv_sec = 10;
		tv.tv_usec = 0;
	    FD_ZERO(&readfds); 
        FD_SET(ad->listenfd, &readfds);
        maxFd = ad->listenfd;
		clientCount = 0;
        for (int i=0; i<fdSize; i++)
        {
            if (fdList[i] > 0)
			{
                FD_SET(fdList[i], &readfds);
				++clientCount;
			}
            if(maxFd < fdList[i])
                   maxFd = fdList[i];
        }  
		if(clientCount>1)
		   ms_warning("client count:::          %d\n",clientCount);    
        result = select(maxFd+1, &readfds, (fd_set *)0,(fd_set *)0, &tv); 
        if(result>0)
		{
			for(int i=0; i<fdSize; i++)
			{
				int fd = fdList[i];
				if(fd<=0)
					continue;
				if (FD_ISSET(fd, &readfds))
				{
					memset(databuf, 0, 2048);
					m_nRecvLen = recv(fd, databuf, 2048, 0);
					ms_warning("audio_read get one package::::::: %d\n", m_nRecvLen);
					if(m_nRecvLen>8)
					{
					   decodeLen = 0;
					   memset(decodebuf, 0, 8192);
					   dealWithAAcaudioData(databuf, m_nRecvLen, decodebuf, &decodeLen);
					   if(decodeLen> 0)
					   {
						   save_send_WavFile(decodebuf, decodeLen);
							om=allocb(decodeLen,0);
							memcpy(om->b_wptr, decodebuf, decodeLen);
							om->b_wptr+=decodeLen;
							ms_mutex_lock(&ad->mutex);
							ms_bufferizer_put(ad->bufferizer,om);
							// ms_warning("decodelen: %d, buffer size:   %d\n",decodeLen, ad->bufferizer->size);
							ms_mutex_unlock(&ad->mutex);
					   }
					} 
					else if(m_nRecvLen<=8&& m_nRecvLen>0)
					{
						ms_warning("receive a wrong package...................==");
					}
					else 
					{
						ms_warning("close one client.....   %d\n", fd);
						close(fd);
						fdList[i] = 0;
					}
				}
			}
			if(FD_ISSET(ad->listenfd,&readfds)) 
			{ 
				int client_sockfd = accept(ad->listenfd, (struct sockaddr *)NULL, NULL); 
				if(client_sockfd >0)
				{
					ms_warning("add one client.....   %d\n", client_sockfd);
					for (int i=0; i<fdSize; i++)
					{
						if (fdList[i] <= 0)
						{
							ad->has_client_connect = TRUE;
							fdList[i] = client_sockfd;
							break;
						}
						
					} 
				}
			} 
		}
		else if(result<0)
		{
			ms_warning("error occur when select................\n");
			break;
		}
		else
		{
			ms_warning("select timeout..............\n");
		}
		
	}
	for(int i= 0; i< fdSize; i++)
    {
		if(fdList[i]>0)
		    close(fdList[i]);
	}
	if(ad->listenfd>0)
	    close(ad->listenfd);
	beforesendCloseFile();
	ms_warning("out t3audio_read_run............fd: %d\n",ad->listenfd); 
	return NULL;
}

void t3audio_read_init(MSFilter *obj){
	T3audioReadData *ad=ms_new0(T3audioReadData,1);
	ms_warning("at t3audio_read_init...");
	ad->rate=(int)sample_rate;
	ad->nchannels=1;
	ad->ticker_synchronizer = ms_ticker_synchronizer_new();
	obj->data=ad;
    ad->has_client_connect = FALSE;
	ad->read_started=FALSE;
	ad->write_started=FALSE;
	ms_mutex_init(&ad->mutex,NULL);
	// ad->thread=0;
	ad->run_thread_quit = FALSE;
	ad->start_read_data = FALSE;
	ad->listenfd = -1;
	ad->connfd = -1;
	ad->bufferizer=ms_bufferizer_new();
	// ms_cond_init(&ad->cond, NULL);
	ms_thread_create(&ad->thread,NULL,t3audio_read_run,ad);
}

void t3audio_read_preprocess(MSFilter *obj){
    
}


void t3audio_read_postprocess(MSFilter *obj){
	T3audioReadData *ad=(T3audioReadData*)obj->data;
	ms_warning("at of t3audio_read_postprocess....");
    ad->run_thread_quit = TRUE;
	ad->start_read_data = FALSE;
	ms_ticker_set_synchronizer(obj->ticker, NULL);
	ad->read_started=FALSE;
	faad_uninit();
	ms_warning("out of t3audio_read_postprocess....");
}

void t3audio_read_uninit(MSFilter *obj){
	T3audioReadData *ad=(T3audioReadData*)obj->data;
	ms_warning("at of t3audio_read_uninit....");
	ad->run_thread_quit = TRUE;
	ad->start_read_data = FALSE;
	// ms_cond_wait(&ad->cond, &ad->mutex);
    ad->has_client_connect = FALSE;
	ms_thread_join(ad->thread,NULL);
	ms_bufferizer_destroy(ad->bufferizer);
	ms_mutex_destroy(&ad->mutex);
	ms_ticker_synchronizer_destroy(ad->ticker_synchronizer);
	 ms_free(ad);
	ms_warning("out of t3audio_read_uninit....");
}



void t3audio_read_process(MSFilter *obj){
	mblk_t *om =NULL;
	T3audioReadData *ad=(T3audioReadData*)obj->data;
	int samples=(160*ad->rate)/8000;
	unsigned int size=samples*2*ad->nchannels;

   
   	ms_mutex_lock(&ad->mutex);
	//    if(!ad->has_client_connect)
	//    {
            
	//   om=allocb(size,0);
	//   om->b_wptr+=size;
	//   /*ms_message("alsa_read_process: Outputing %i bytes",size);*/
	//   ms_queue_put(obj->outputs[0],om);
	//    }
	//    else
	//    {
	while (ms_bufferizer_get_avail(ad->bufferizer)>=size){

	  om=allocb(size,0);
	  ms_bufferizer_read(ad->bufferizer,om->b_wptr,size);
	  om->b_wptr+=size;
	  /*ms_message("alsa_read_process: Outputing %i bytes",size);*/
	  ms_queue_put(obj->outputs[0],om);
	}
	//    }
	ms_mutex_unlock(&ad->mutex);
}



static int t3audio_read_get_sample_rate(MSFilter *obj, void *param){
	T3audioReadData *ad=(T3audioReadData*)obj->data;
	*((int*)param)=ad->rate;
	return 0;
}


static int t3audio_read_get_nchannels(MSFilter *obj, void *param) {
	T3audioReadData *ad = (T3audioReadData *)obj->data;
	*((int *)param) = ad->nchannels;
	return 0;
}


MSFilterMethod t3audio_read_methods[]={
	{MS_FILTER_GET_SAMPLE_RATE,	t3audio_read_get_sample_rate},
	{MS_FILTER_GET_NCHANNELS, t3audio_read_get_nchannels},
	{0,NULL}
};

MSFilterDesc t3audio_read_desc={
	.id=MS_T3AUDIO_READ_ID,
	.name="MST3AudioRead",
	.text=N_("t3audio sound source"),
	.category=MS_FILTER_OTHER,
	.ninputs=0,
	.noutputs=1,
	.init=t3audio_read_init,
	.preprocess=t3audio_read_preprocess,
	.process=t3audio_read_process,
	.postprocess=t3audio_read_postprocess,
	.uninit=t3audio_read_uninit,
	.methods=t3audio_read_methods
};

static MSFilter * ms_t3audio_read_new(MSFactory *factory, const char *dev){
	MSFilter *f=ms_factory_create_filter_from_desc(factory, &t3audio_read_desc);
	return f;
}


struct _T3audioWriteData{
	int rate;
	int nchannels;
	bool_t write_started; 
	ms_mutex_t mutex;
	int connfd;
};

typedef struct _T3audioWriteData T3audioWriteData;


static int init_t3audio_write_socket()
{
    struct sockaddr_in serveraddr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
		return sockfd;
    }
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(audio_write_port);
    serveraddr.sin_addr.s_addr=inet_addr(audio_write_ip);
 
    if(connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
    {
	    ms_warning("init_t3audio_write_socket failed.......");
	    return -1;
    }
	else
	{
		 ms_warning("init_t3audio_write_socket success.......");
	}
    return sockfd;
}

static unsigned int totalLen = 0;
static void initHeader()
{
	 int bits = 16;
	ms_warning("at initHeader....." );
    memcpy(header.riff.riff, "RIFF",4);
    memcpy(header.riff.wave, "WAVE",4);
    memcpy(header.format.fmt, "fmt ", 4);
    header.format.len = 0x10;
    header.format.type = 0x1;
    header.format.channel = 1;
    header.format.rate = 8000;
    header.format.bps = 8000 * bits/8 * 1;
    header.format.blockalign = bits/8 * 1;
    header.format.bitpspl = bits;

    memcpy(header.data.data, "data", 4);
	ms_warning("out initHeader....." );
}
static void updateHeader()
{
    header.data.len = (uint32_t)totalLen;
	header.riff.len = header.data.len + 36;
}
static void beforeCloseFile()
{
	ms_warning("at before closeFile..............");
	if(!firstWriteFile)
	{
    updateHeader();
    fseek(fp, 0, SEEK_SET);
    fwrite(&header, sizeof(header),1, fp);
    fclose(fp);
	fp = NULL;
	firstWriteFile = TRUE;
	}
}


static void save_WavFile(char * data, const int len)
{
    if(firstWriteFile)
    {
        if ((fp = fopen(fileName, "wb")) == NULL) 
	    {
            printf("create wav file error\n");
            return;
	    }
        initHeader();
        fwrite(&header, sizeof(header),1, fp);
		fflush(fp);
        firstWriteFile = FALSE;
    }
    fwrite(data, len,1, fp);
	fflush(fp);
    totalLen += len;
}


void t3audio_write_init(MSFilter *obj){
	T3audioWriteData *ad=ms_new0(T3audioWriteData,1);
	ms_warning("at t3audio_write_init...");
	ad->rate=forced_rate!=-1 ? forced_rate : 8000;
	ad->nchannels=1;
	ad->write_started = FALSE;
	ad->connfd = init_t3audio_write_socket();
	obj->data=ad;
}

void t3audio_write_postprocess(MSFilter *obj){
	T3audioReadData *ad=(T3audioReadData*)obj->data;
	ad->write_started=FALSE;
	ms_warning("at t3audio_write_postprocess...");
}

void t3audio_write_uninit(MSFilter *obj){
	T3audioWriteData *ad=(T3audioWriteData*)obj->data;
	ms_warning("at t3audio_write_uninit...");
    ad->write_started = FALSE;
	beforeCloseFile();
	if(ad->connfd > 0)
	    close(ad->connfd);
	ms_free(ad);
}

static int t3audio_write_get_sample_rate(MSFilter *obj, void *data){
	T3audioWriteData *ad=(T3audioWriteData*)obj->data;
	*((int*)data)=ad->rate;
	return 0;
}


int t3audio_write_get_nchannels(MSFilter *obj, void *data) {
	T3audioWriteData *ad = (T3audioWriteData *)obj->data;
	*((int *)data) = ad->nchannels;
	return 0;
}



void t3audio_write_process(MSFilter *obj){
	mblk_t *im =NULL;
	int size;
	int samples;
	int msgsize;
	T3audioWriteData *ad=(T3audioWriteData*)obj->data;
	// ms_warning("at... of ... t3audio_write_process....\n");
	if(ad->connfd <= 0)
	   ad->connfd = init_t3audio_write_socket();
	ad->write_started = TRUE;
	while ((im=ms_queue_get(obj->inputs[0]))!=NULL){
		size=im->b_wptr-im->b_rptr;
		save_WavFile((char*)im->b_rptr, size);
		 msgsize = msgdsize(im);
		ms_warning("get package len:::::::::::::: %d, msgsize: %d", size, msgsize);
		while((size=im->b_wptr-im->b_rptr)>0){
			// samples=size/(2*ad->nchannels);
			samples=size;
			if(ad->connfd >0)
			{
			    int dataSize = send(ad->connfd, im->b_rptr, samples, 0);
				if(dataSize > 0)
					im->b_rptr+= dataSize;
				else 
				    break;
			}
			else break;
		}
		freemsg(im);
	}
	// ms_warning("out of ... t3audio_write_process....\n");
}

MSFilterMethod t3audio_write_methods[]={
	{MS_FILTER_GET_SAMPLE_RATE,	t3audio_write_get_sample_rate},
	{MS_FILTER_GET_NCHANNELS, t3audio_write_get_nchannels},
	{0,NULL}
};

MSFilterDesc t3audio_write_desc={
	.id=MS_T3AUDIO_WRITE_ID,
	.name="MST3AudioWrite",
	.text=N_("t3audio sound output"),
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=0,
	.init=t3audio_write_init,
	.process=t3audio_write_process,
	.postprocess=t3audio_write_postprocess,
	.uninit=t3audio_write_uninit,
	.methods=t3audio_write_methods
};


static MSFilter * ms_t3audio_write_new(MSFactory *factory, const char *dev){
	MSFilter *f = ms_factory_create_filter_from_desc(factory, &t3audio_write_desc);
	return f;
}


MS_FILTER_DESC_EXPORT(t3audio_write_desc)

MS_FILTER_DESC_EXPORT(t3audio_read_desc)

