#include <net/if.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>

//defining macros to be used in program
#define BUF_MAX_SIZE 256
#define multi_addressESS "239.192.1.10"
#define SERVER_PORT 5432 
#define BUF_SIZE 2048
#define BUF_SIZE_SMALL 256 

//song information structure
typedef struct song_det_t 
{

	uint8_t song_no;
	uint8_t song_store_sz;
	char song_store_nm[BUF_MAX_SIZE];
	uint32_t multi_cast_addr;
	uint16_t d_port;
	uint16_t in_port;
	uint32_t bit_rate;
} song_det;

//song information request structure
typedef struct song_det_req_t {
	uint8_t type;   
} song_det_req;

//song_det_req_t function
song_det_req initSongDetReq(song_det_req *s_i_req)
{
	s_i_req->type = 1;
	return *s_i_req;
}

//site information structure
typedef struct song_store_det_t 
{ 
	uint8_t type;
	uint8_t song_store_nm_sz;
	char song_store_nm[BUF_MAX_SIZE];
	uint8_t song_store_dsc_sz;
	char song_store_dsc[BUF_MAX_SIZE];
	uint8_t song_counter;
	song_det songs_lst[BUF_MAX_SIZE];
} song_store_det;

//site_info function
song_store_det initSongStoreInformation(song_store_det *s_p_i)
{
	s_p_i->type = 10;	
	return *s_p_i;
}

// structure for song finding failed
typedef struct song_fail_t
{
	uint8_t type;
	uint8_t song_no;
} song_fail;

//song_fail_t structure
song_fail initSongFail(song_fail *song_fail)
{
	song_fail->type = 11;
	return *song_fail;
}

//Current and next playing songs structure
typedef struct play_song_det_t
{
	uint8_t type;   // = 12;
	uint8_t play_song_nm_sz;
	char play_song_nm[BUF_MAX_SIZE];
	uint16_t rem_tm_scnd;
	uint8_t play_song_next_nm_sz;
	char play_song_next_nm[BUF_MAX_SIZE];
} play_song_det;

//play_song_det_t structure
play_song_det initPlaySongDet(play_song_det *s_p_i)
{
	s_p_i->type = 12;
	return *s_p_i;
}

//variable declration
int typeChange = 0;
int total_no_of_songs;
song_det songs_store[25];

int curVLCPid = 0;
int stationNow = 0;
int count = 0;

int arg_counter;
int forceClose = 0;
void ReceiveSongs(void*);
pthread_t recvSongsPID;


char cur_status = 'r';

// Function for receiving songs by multicasting
void startFNB(char* argv[])
{
    pthread_create(&recvSongsPID, NULL, &ReceiveSongs, argv);

    
    pthread_join(recvSongsPID, NULL);
}

// Delete temp made songs
void removTemp()
{
    system("rm tempSong*");
}

// Stops the running process, used in pause, stop and selecting other song operations
void* stop(void* args)
{
    char pidC[10];  
    char command[256];
    
    //Null values assigning to pidC and command 
    memset(command, '\0', 256);
    memset(pidC, '\0', 10);
    
    strcpy(command, "kill ");
    
    sprintf(pidC, "%d", curVLCPid);

    //append pidC 
    memcpy(&command[strlen("kill ")], pidC, strlen(pidC));
    
    system(command);
    return NULL;
   
}

// display song list
void receiveSongList(char * argv[])
{
    int sT; 
    struct sockaddr_in s_addr;
    char* SERVER_ADDRESS;
    SERVER_ADDRESS = argv[1];
    
   
    if ((sT = socket(PF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("Failed to craete soket");
        exit(1);
    }

    // Initialise address variables
    bzero((char *) &s_addr, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);;
    s_addr.sin_port = htons(SERVER_PORT);
    
    // socket to server connection
    int st = -1;
    while(st < 0)
    {
        st = connect(sT, (struct sockaddr *) &s_addr, sizeof(s_addr));    
    } 
    
    char buffer1[BUF_SIZE_SMALL];

    
    send(sT, "fnb.txt\n", 9, 0);
    
    uint32_t no_of_songs = 0;

    read(sT, &no_of_songs, sizeof(uint32_t));
    no_of_songs = ntohl(no_of_songs);
    total_no_of_songs = no_of_songs;
    
    //checking songs limit 
    printf("available songs, number =  %d\n", no_of_songs);
    if(no_of_songs > 16)
    {
        printf("total song number exceeded\n");
        exit(0);
    }
    
    int j=0;
    song_det* si = malloc(sizeof(song_det));
    
    // display song details
    for(j=0;j<no_of_songs;j++)
    {
        read(sT, si, sizeof(song_det));
	printf("\n******** SONG INFORMATION %d ******** ", j);
        printf("\nsong number : %hu\n", si->song_no);
	printf("\nsong multicast port : %d", ntohs(si->d_port));
        printf("\n\nNaem of song : %s\n", si->song_store_nm);
        
        memcpy(&songs_store[j], si, sizeof(song_det));
    }

    close(sT);
}

// receive song selected by UDP stream 
void ReceiveSongs(void* args)
{
    printf("\n\nsongs receiving");
    char** argv = (char**)args;
    
    int s;                  
    struct sockaddr_in sin; 
    char *conn_name;          
    struct ifreq ifr;       
    char buffer1[BUF_SIZE];
    int len;

    // Multicasting details
    char *multi_address;               
    struct ip_mreq mcast_req;       
    struct sockaddr_in mcast_saddr; 
    socklen_t mcast_saddr_len;
    
    if(arg_counter == 3) {
        conn_name = argv[2];
    }
    else
        conn_name = "wlan0";
    int mc_port;
    
    // socket creation
    if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("\nFailed to create soket");
        exit(1);
    } 
    
    // assign specific port to specific song

    
   if(stationNow > total_no_of_songs)
    {
	// play song one for invalid song selection
        printf("\ninvalid song number... redirecting to first song");
        mc_port = 2300;
        stationNow = 0;

    }   
       else {
	
	// for acceptable song number, assign specific port
        mc_port = ntohs(songs_store[stationNow].d_port);
    }
    
    // define addres variables
    memset((char *)&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(mc_port);
    
    
     
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name , conn_name, sizeof(conn_name)-1);
    
    if ((setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (void *)&ifr, sizeof(ifr))) < 0)
    {
        perror("unsuccessful socket creation");
        close(s);
        exit(1);
    }
    
    // binding 
    if ((bind(s, (struct sockaddr *) &sin, sizeof(sin))) < 0) 
    {
        perror("error while binding socket");
        exit(1);
    }
    
    

    mcast_req.imr_multiaddr.s_addr = inet_addr(multi_addressESS);
    mcast_req.imr_interface.s_addr = htonl(INADDR_ANY);
    
    // multicasting status 
    if ((setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*) &mcast_req, sizeof(mcast_req))) < 0) 
    {
        perror("unsuccessful multicast joining");
        exit(1);
    }
    
    // receive 
    FILE* file;
    int counter = 0;
    count=0;

    printf("\nwelcome to FNB listen !!!\nhave a great music experience");

    play_song_det* songInformation = (play_song_det*) malloc(sizeof(play_song_det));
    char* temp_songs_store[2];
    temp_songs_store[0] = "tempSong1.mp3";
    temp_songs_store[1] = "tempSong2.mp3";
    
    int typeChange = 0, cur=0;
    char* tempSong = temp_songs_store[cur];

    while(1) 
    {
        if(typeChange) 
	{
            tempSong = temp_songs_store[1 ^ cur];
        }
        
        // mcast_saddr = 0
        memset(&mcast_saddr, 0, sizeof(mcast_saddr));
        mcast_saddr_len = sizeof(mcast_saddr);
        
        // buffer1=0 
        memset(buffer1, 0, sizeof(buffer1));
        if ((len = recvfrom(s, buffer1, BUF_SIZE, 0, (struct sockaddr*)&mcast_saddr, &mcast_saddr_len)) < 0) 
	{
            perror("error while receiving");
            exit(1);
        } else {
            count = count + len;
       
            if(count >= 387000) 
	    {
         	// disconnect thread
		printf("\nplaying the songs\n");
                pthread_detach(pthread_self());
                pthread_exit(NULL);
                
            }
        }
        
        // streaming info
        uint8_t tempType;
        if(len == sizeof(play_song_det)) 
	{
            printf("Len = %d. Checking if songInfo...\n", len);
            memcpy(songInformation, buffer1, len);

            printf("type of Song info = %hu\n", songInformation->type);
            tempType = (uint8_t)songInformation->type;
            
	    // type = 12 means song info structure
            if(tempType == 12)
	    {
		// printing current song and next song name
                printf("Current Song : %s\n", songInformation->play_song_nm);
                printf("Next Song : %s\n", songInformation->play_song_next_nm_sz);
                continue;
            }
            else {
                printf("Not here!\n");
	    }
        }
        
        if(counter++ == 10)
	{
            curVLCPid = fork();
            if(curVLCPid == 0)
	    {
		
                execlp("/usr/bin/cvlc", "cvlc", tempSong, (char*) NULL);



		
            }
        }
        
        file = fopen(tempSong, "ab+");
        fwrite(buffer1, len, 1, file);
        fclose(file);
        
         
        if(forceClose == 1)
            break;
    }
    
    close(s);
    forceClose = 0;
    file = fopen(tempSong, "wb");
    fclose(file);
}

// Play button - play selected or default song
void clickedPlayButton(GtkWidget *widget, gpointer data, char * argv[]) 
{
    printf("\n\nLet's listen");
    startFNB(argv);
    cur_status = 'r';
}

// Pause button - pause the currently playing song
void clickedPauseButton(GtkWidget *widget, gpointer data) 
{
    printf("\n\nsong paused");
    stop(NULL);

    forceClose = 1;
    
    cur_status = 'p';
}
// Stop button - Stop the application FNB
void clickedStopButton(GtkWidget *widget, gpointer data) 
{
    g_print("\nsongs stopped\n");
    stop(NULL);
    forceClose = 1;
    removTemp();
    exit(0);
}

// Action listener for song 1
void clickSong1(GtkWidget *widget, gpointer data, char * argv[]) 
{
	g_print("Song 1 preparing...\n");
	stop(NULL);
        forceClose = 1;
        removTemp();
        int station=1;
        stationNow = station - 1;
        cur_status = 'c';
	//gtk_window_close(widget);
	
}

// Action listener for song 2
void clickSong2(GtkWidget *widget, gpointer data, char * argv[]) 
{
	g_print("Song 2 preparing...\n");
	stop(NULL);
        forceClose = 1;
        removTemp();
        int station=2;
        stationNow = station - 1;
        cur_status = 'c';
}

// Action listener for song 3
void clickSong3(GtkWidget *widget, gpointer data, char * argv[]) 
{
	g_print("Song 3 preparing...\n");
	stop(NULL);
        forceClose = 1;
        removTemp();
        int station=3;
        stationNow = station - 1;
        cur_status = 'c';
}


// Action listener for song 4
void clickSong4(GtkWidget *widget, gpointer data, char * argv[]) 
{
	g_print("Song 4 preparing...\n");
	stop(NULL);
        forceClose = 1;
        removTemp();
        int station=4;
        stationNow = station - 1;
        cur_status = 'c';
}


// Action listener for song 5
void clickSong5(GtkWidget *widget, gpointer data, char * argv[]) 
{
	g_print("Song 5 preparing...\n");
	stop(NULL);
        forceClose = 1;
        removTemp();
        int station=5;
        stationNow = station - 1;
        cur_status = 'c';
}


// Action listener for song 6
void clickSong6(GtkWidget *widget, gpointer data, char * argv[]) 
{
	g_print("Song 6 preparing...\n");
	stop(NULL);
        forceClose = 1;
        removTemp();
        int station=6;
        stationNow = station - 1;
        cur_status = 'c';
}


// Action listener for song 7
void clickSong7(GtkWidget *widget, gpointer data, char * argv[]) 
{
	g_print("Song 7 preparing...\n");
	stop(NULL);
        forceClose = 1;
        removTemp();
        int station=7;
        stationNow = station - 1;
        cur_status = 'c';
}


// Action listener for song 8
void clickSong8(GtkWidget *widget, gpointer data, char * argv[]) 
{
	g_print("Song 8 preparing...\n");
	stop(NULL);
        forceClose = 1;
        removTemp();
        int station=8;
        stationNow = station - 1;
        cur_status = 'c';
}


// Action listener for song 9
void clickSong9(GtkWidget *widget, gpointer data, char * argv[]) 
{
	g_print("Song 9 preparing...\n");
	stop(NULL);
        forceClose = 1;
        removTemp();
        int station=9;
        stationNow = station - 1;
        cur_status = 'c';
}


// Action listener for song 10
void clickSong10(GtkWidget *widget, gpointer data, char * argv[]) 
{
	g_print("Song 10 preparing...\n");
	stop(NULL);
        forceClose = 1;
        removTemp();
        int station=10;
        stationNow = station - 1;
        cur_status = 'c';
}



// Select Song button: Show song listt
void clickedSelectButton(char* argv[])
{
    // First screen GUI var
    GtkWidget *window;
    GtkWidget *halign;
    GtkWidget *vbox;
    GtkWidget *button1;
    GtkWidget *button2;
    GtkWidget *button3;
    GtkWidget *button4;
    GtkWidget *button5;
    GtkWidget *button6;
    GtkWidget *button7;
    GtkWidget *button8;
    GtkWidget *button9;
    GtkWidget *button10;
    GdkColor colour1;
    GdkColor colour2;
    GdkColor colour3;
    GtkWidget *frame;
    GtkWidget *label;

    int argc = 1;
    gtk_init(&argc, &argv);
    char o;
    char lo = 'r'; 
    int station, f=1;
    
    // Set first screen
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "FNB LISTEN");
    gtk_window_set_default_size(GTK_WINDOW(window), 1500, 300);
    gtk_container_set_border_width(GTK_CONTAINER(window), 15);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    vbox = gtk_vbox_new(TRUE,1);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Buttons on select song screen
    frame = gtk_frame_new ("");
    label = gtk_label_new ("SONG LIST");
  
    gtk_container_add (GTK_CONTAINER (frame), label);
    gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);    
    
    //Song names on the 10 song list buttons
    
    button1 = gtk_button_new_with_label("Buddhu Sa Mann");
    button2 = gtk_button_new_with_label("Aayat");
    button3 = gtk_button_new_with_label("Abhi Na Jao Chhod Kar");
    button4 = gtk_button_new_with_label("Dinag Dang");
    button5 = gtk_button_new_with_label("Gerua");
    button6 = gtk_button_new_with_label("Humdard");
    button7 = gtk_button_new_with_label("Luv U Zindagi");
    button8 = gtk_button_new_with_label("Chalti Hai Kya 9 Se 12");
    button9 = gtk_button_new_with_label("Neele Neele Ambar Par");
    button10 = gtk_button_new_with_label("Saturday Saturday");
    // button sizes
    
    gtk_widget_set_size_request(button1, 90, 40);
    gtk_widget_set_size_request(button2, 90, 40);
    gtk_widget_set_size_request(button3, 90, 40);
    gtk_widget_set_size_request(button4, 90, 40);
    gtk_widget_set_size_request(button5, 90, 40);
    gtk_widget_set_size_request(button6, 90, 40);
    gtk_widget_set_size_request(button7, 90, 40);
    gtk_widget_set_size_request(button8, 90, 40);
    gtk_widget_set_size_request(button9, 90, 40);
    gtk_widget_set_size_request(button10, 90, 40);    
    
	//set buttons for song list 
    
    gtk_box_pack_start(GTK_BOX(vbox), button1, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button2, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button3, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button4, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button5, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button6, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button7, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button8, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button9, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button10, TRUE, TRUE, 0);

 
    gtk_widget_modify_text ( GTK_WIDGET(label), GTK_STATE_NORMAL, &colour3);      

    // assign calling methods upon select song(button click) event
    g_signal_connect(G_OBJECT(button1), "clicked", G_CALLBACK(clickSong1), argv);
    g_signal_connect(G_OBJECT(button2), "clicked", G_CALLBACK(clickSong2), argv);
    g_signal_connect(G_OBJECT(button3), "clicked", G_CALLBACK(clickSong3), argv);
    g_signal_connect(G_OBJECT(button4), "clicked", G_CALLBACK(clickSong4), argv);
    g_signal_connect(G_OBJECT(button5), "clicked", G_CALLBACK(clickSong5), argv);
    g_signal_connect(G_OBJECT(button6), "clicked", G_CALLBACK(clickSong6), argv);
    g_signal_connect(G_OBJECT(button7), "clicked", G_CALLBACK(clickSong7), argv);
    g_signal_connect(G_OBJECT(button8), "clicked", G_CALLBACK(clickSong8), argv);
    g_signal_connect(G_OBJECT(button9), "clicked", G_CALLBACK(clickSong9), argv);
    g_signal_connect(G_OBJECT(button10), "clicked", G_CALLBACK(clickSong10), argv);

    g_signal_connect(G_OBJECT(window), "destroy",G_CALLBACK(gtk_main_quit), G_OBJECT(window));
    
    // make screen visible
    gtk_widget_show_all(window);
    gtk_main();
}


// driver code

int main(int argc, char * argv[])
{
    arg_counter = argc;
   
    // Receiving and printing Station List coming from server
    receiveSongList(argv);

    // Initializing Variables for GUI main window
    GtkWidget *window;
    GtkWidget *halign;
    GtkWidget *vbox;
    GtkWidget *button;
    GtkWidget *button1;
    GtkWidget *button2;
    GtkWidget *button3;
    GdkColor colour1;
    GdkColor colour2;

    gtk_init(&argc, &argv);

    printf("\nIn Main Function\n");
    char o;
    char lo = 'r';
    int station, f=1;
    
     // Assigning colors to variables
    gdk_color_parse ("blue", &colour1);
    gdk_color_parse ("yellow", &colour2);
    

    // set screen and show buttons
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "FNB LISTEN");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 300);
    gtk_container_set_border_width(GTK_CONTAINER(window), 15);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    vbox=gtk_vbox_new(TRUE,1);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    //name of buttons to play, pause,select song and exit
    button = gtk_button_new_with_label("Play");
    button1 = gtk_button_new_with_label("Pause");
    button2 = gtk_button_new_with_label("Select Song");
    button3 = gtk_button_new_with_label("Exit");
    
    // assign buttons sizes
    gtk_widget_set_size_request(button, 90, 40);
    gtk_widget_set_size_request(button1, 90, 40);
    gtk_widget_set_size_request(button2, 90, 40);
    gtk_widget_set_size_request(button3, 90, 40);
    
    gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button1, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button2, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button3, TRUE, TRUE, 0);

     // assign calling methods to buttons for button clicked event
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(clickedPlayButton), argv);
    g_signal_connect(G_OBJECT(button1), "clicked", G_CALLBACK(clickedPauseButton), NULL);
    g_signal_connect(G_OBJECT(button2), "clicked", G_CALLBACK(clickedSelectButton), argv);
    g_signal_connect(G_OBJECT(button3), "clicked", G_CALLBACK(clickedStopButton), NULL);

    g_signal_connect(G_OBJECT(window), "destroy",G_CALLBACK(gtk_main_quit), G_OBJECT(window));
    
    gtk_widget_show_all(window);
    
    gtk_main();

    return 0;
}
