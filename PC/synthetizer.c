#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <termios.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define POLY 16
#define GAIN 5000.0
#define BUFSIZE 512

snd_pcm_t *playback_handle;
short* buffer;

double phi[POLY], phi_mod[POLY], pitch, modulation, velocity[POLY], attack, decay, sustain, release, env_time[POLY], env_level[POLY];
int harmonic, subharmonic, transpose, note[POLY], gate[POLY], note_active[POLY], rate;




//-----------------------------------------------------------------DEFINIZIONE THREAD-------------------------------------------------------//
typedef struct handler_audio_args_s {
    snd_pcm_t* playback_handle;

} handler_audio_args_t;

typedef struct handler_serial_args_s {
    int keyboard_fd;

} handler_serial_args_t;

void audio_function_thread(void *arg){
    handler_audio_args_t* args = (handler_audio_args_t*) arg;

    while(1){
        if(note_active[16] == 1){
            break;
        }
        if (playback_callback(BUFSIZE) < BUFSIZE) {
            fprintf (stderr, "xrun !\n");
            snd_pcm_prepare(args -> playback_handle);
        }
    }
    
    free(buffer);
    snd_pcm_close (playback_handle);
    printf("[Audio thread] End activity...\n");
    pthread_exit(NULL);

}

void serial_function_thread(void *arg){
    handler_serial_args_t* args = (handler_serial_args_t*) arg;
    int ok;
    while(1){
        ok = 1;
        int bsize=3;
        char* buffer_read = malloc(sizeof(char)*bsize);
        int n_read;
        n_read = read(args -> keyboard_fd, buffer_read, 3);
        buffer_read[n_read] = '\0';
        if(n_read < 3 || strlen(buffer_read) < 3) continue;
        int i = 0;
        for(i = 0; i<strlen(buffer_read); i++){
            if(buffer_read[i] < 48 || buffer_read[i] > 57){
                ok=0;
             }
        }
        if(!ok) continue;
        printf("[Serial thread] Ho letto: %s  <------> nbyte: %d\n", buffer_read,strlen(buffer_read));

        

        //is on?
        short on;
        if(buffer_read[0] == 48){
            on = 0;
        }else{
            on = 1;
        }

        //key pin
        char* number = malloc(sizeof(char)*2);
        for(i=0; i<2;i++){
            number[i] = buffer_read[i+1];
        }
        short key = atoi(number);


        //check quit button
        if(key == 16 && on == 1){
            note_active[key] = 1;
            break;
        }

	if(key == 16 && on == 0){
		continue;
	}

        //Note on-off
        if(on){
          note[key] = key+89;
            velocity[key] = 127.0 / 127.0;
            env_time[key] = 0;
            gate[key] = 1;
            note_active[key] = 1;
        }else{
            note_active[key] = 0;
            note[key] = key+89;
        }


    }

    close(args -> keyboard_fd);
    printf("[Serial thread] End activity...\n");
    pthread_exit(NULL);
}



//----------------------------------------------------------------------------------------------------------------------------------------------//



//------------------------------------------------------------DEFINIZIONE FUNZIONI SERIALE------------------------------------------------------//
int keyboard_open(const char* device){
    int fd = open (device, O_RDWR | O_NOCTTY | O_SYNC );
    if(fd == -1){
        perror("Errore apertura keyboard\n");
        exit(EXIT_FAILURE);
    }
    return fd;
}

int serial_set_interface_attribs(int fd, int speed, int parity){
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr (fd, &tty) != 0){
        printf("error %d from tcgetattr", errno);
        return -1;
    }
    switch(speed){
        case 19200:
            speed=B19200;
            break;
        case 57600:
            speed = B57600;
            break;
        case 115200:
            speed=B115200;
            break;
        default:
            printf("Cannot sed baudrate %d\n", speed);
            return -1;
    }
    cfsetospeed (&tty, speed);
    cfsetispeed (&tty, speed);
    cfmakeraw(&tty);
    //enable reading
    tty.c_cflag &= ~(PARENB | PARODD); //shut off parity
    tty.c_cflag |= parity;
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; //8-bit chars

    if(tcsetattr(fd,TCSANOW,&tty) != 0 ) {
        printf("error %d from tcsetaddr\n", errno);
        return -1;
    }
    return 0;

}

//------------------------------------------------------------------------------------------------------------------------//



//---------------------------------DEFINIZIONE PCM ---------------------------------------------------------------------//

snd_pcm_t *open_pcm(char *pcm_name) {

    snd_pcm_t *playback_handle;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;

    if (snd_pcm_open (&playback_handle, pcm_name, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf (stderr, "cannot open audio device %s\n", pcm_name);
        exit (1);
    }
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(playback_handle, hw_params);
    snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rate, 0);
    snd_pcm_hw_params_set_channels(playback_handle, hw_params, 2);
    snd_pcm_hw_params_set_periods(playback_handle, hw_params, 2, 0);
    snd_pcm_hw_params_set_period_size(playback_handle, hw_params, BUFSIZE, 0);
    snd_pcm_hw_params(playback_handle, hw_params);
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(playback_handle, sw_params);
    snd_pcm_sw_params_set_avail_min(playback_handle, sw_params, BUFSIZE);
    snd_pcm_sw_params(playback_handle, sw_params);
    return(playback_handle);
}

double envelope(int *note_active, int gate, double *env_level, double t, double attack, double decay, double sustain, double release) {

    if (gate)  {
        if (t > attack + decay) return(*env_level = sustain);
        if (t > attack) return(*env_level = 1.0 - (1.0 - sustain) * (t - attack) / decay);
        return(*env_level = t / attack);
    } else {
        if (t > release) {
            if (note_active) *note_active = 0;
            return(*env_level = 0);
        }
        return(*env_level * (1.0 - t / release));
    }
}



int playback_callback (snd_pcm_sframes_t nframes) {

    int l1, l2;
    double dphi, dphi_mod, f1, f2, f3, freq_note, sound;

    memset(buffer, 0, nframes * 4);
    for (l2 = 0; l2 < POLY; l2++) {
        if (note_active[l2]) {
            f1 = 8.176 * exp((double)(transpose+note[l2]-2)*log(2.0)/12.0);
            f2 = 8.176 * exp((double)(transpose+note[l2])*log(2.0)/12.0);
            f3 = 8.176 * exp((double)(transpose+note[l2]+2)*log(2.0)/12.0);
            freq_note = (pitch > 0) ? f2 + (f3-f2)*pitch : f2 + (f2-f1)*pitch;
            dphi = M_PI * freq_note / 22050.0;
            dphi_mod = dphi * (double)harmonic / (double)subharmonic;
            for (l1 = 0; l1 < nframes; l1++) {
                phi[l2] += dphi;
                phi_mod[l2] += dphi_mod;
                if (phi[l2] > 2.0 * M_PI) phi[l2] -= 2.0 * M_PI;
                if (phi_mod[l2] > 2.0 * M_PI) phi_mod[l2] -= 2.0 * M_PI;
                sound = GAIN * envelope(&note_active[l2], gate[l2], &env_level[l2], env_time[l2], attack, decay, sustain, release)
                             * velocity[l2] * sin(phi[l2] + modulation * sin(phi_mod[l2]));
                env_time[l2] += 1.0 / 44100.0;
                buffer[2 * l1] += sound;
                buffer[2 * l1 + 1] += sound;
            }
        }
    }
    return snd_pcm_writei (playback_handle, buffer, nframes);
}

//--------------------------------------------------------------------------------------------------------------------------------------------//


//----------------------------------------------------MAIN---------------------------------------------------------------------------------------//
int main (int argc, char *argv[]) {

    int l1;

    pitch = 0;

    modulation = atof("0.7");
    harmonic = atoi("1");
    subharmonic = atoi("3");
    transpose = atoi("24");
    attack = atof("0.05");
    decay = atof("0.3");
    sustain = atof("0.8");
    release = atof("0.2");

    //open keyboard
    const char* dev = "/dev/ttyACM0";
    int keyboard_fd = keyboard_open(dev);

    //set attributi seriale
    if(serial_set_interface_attribs(keyboard_fd, 19200, 0) == -1){
        printf("error %d from serial_set_interface_attribs\n",  errno);
        return -1;
    }

    //allocazione buffer playback
    buffer = (short *) malloc (2 * sizeof (short) * BUFSIZE);

    //open pcm
    playback_handle = open_pcm("plughw:0,0");

    for (l1 = 0; l1 < POLY; note_active[l1++] = 0);


    int ret = 0;
    pthread_t audio_thread, serial_thread;
    handler_audio_args_t* audio_args = malloc(sizeof(handler_audio_args_t));
    audio_args -> playback_handle = playback_handle;

    handler_serial_args_t* serial_args = malloc(sizeof(handler_serial_args_t));
    serial_args -> keyboard_fd = keyboard_fd;

    ret = pthread_create(&audio_thread, NULL, audio_function_thread ,(void*)audio_args);
    if(ret == -1){
        fprintf(stderr, "Errore creazione audio_thread\n");
    }
    ret = pthread_create(&serial_thread, NULL, serial_function_thread ,(void*)serial_args);
    if(ret == -1){
        fprintf(stderr, "Errore creazione serial_thread\n");
    }



    pthread_join(serial_thread, NULL);
    pthread_join(audio_thread, NULL);



    pthread_detach(serial_thread);
    pthread_detach(audio_thread);





    printf("[MAIN] End process ..\n");
    
    return (0);
}
