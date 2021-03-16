#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <dirent.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

/*Defining all the variables used recurrently in the program*/
#define MAX_BUFFER_SIZE 256
#define MULTI_SERVER_IP "192.168.43.69"
#define BUFFER_SIZE 2048
#define SERVER_PORT 5432
#define BUFFER_SIZE_SMALL 1024
#define NO_OF_STATIONS 10

//Defining structure for station info request
typedef struct song_info_req_t {
    uint8_t type;
} song_info_req;

//Init Function for song_info_req_t
song_info_req initSongInfoReq(song_info_req *s_i_req){
    s_i_req->type = 1;
    return *s_i_req;
}

//Defining structure for station info
typedef struct song_info_t {
    uint8_t song_no;
    uint8_t song_store_size;
    char song_store_name[MAX_BUFFER_SIZE];
    uint32_t multicast_address;
    uint16_t data_port;
    uint16_t information_port;
    uint32_t bit_rate;
} song_info;

//Defining structure for site info
typedef struct site_information_t {
    uint8_t type;// = 10;
    uint8_t site_nm_size;
    char site_nm[MAX_BUFFER_SIZE];
    uint8_t site_dsc_size;
    char site_dsc[MAX_BUFFER_SIZE];
    uint8_t song_counter;
    song_info songs_list[MAX_BUFFER_SIZE];
} site_information;

//Init Function for song_info
site_information initSiteInformation(site_information *si){
    si->type = 10;
    return *si;
}

//Defining structure for station not found
typedef struct song_fail_t{
    uint8_t type;// = 11;
    uint8_t song_no;
} song_fail;

//Init Function for song_fail
song_fail initSongFail(song_fail *song_fail){
    song_fail->type = 11;
    return *song_fail;
}

//Defining structure for song info
typedef struct song_play_info_t {
    uint8_t type;// = 12;
    uint8_t song_play_name_size;
    char song_play_name[MAX_BUFFER_SIZE];
    uint16_t rem_time_second;
    uint8_t next_song_play_name_size;
    char next_song_play_name[MAX_BUFFER_SIZE];
} song_play_info;

//Init Function for song_play_info
song_play_info initSongPlayInfo(song_play_info *s_p_i){
    s_p_i->type = 12;
    return *s_p_i;
}

//Defining structure for station id path
typedef struct song_no_store_path_t
{
    int no;
    char path[BUFFER_SIZE_SMALL];
    int port;
} song_no_store_path;

//Creating array of objects of structures
song_info songs_store[NO_OF_STATIONS];
song_no_store_path songnostorepath[NO_OF_STATIONS];

//Declaring threads for stations
pthread_t songstorethread[NO_OF_STATIONS];

//Declaring sleep variable
long idle_sleep;

//Declaring variables for storing command line arguments
int argC;
char **argV;
//Function for getting the bit rate
int getBitRate(char *fName){
    int PID = 0;
    char *command1 = "mediainfo "; //Fetching all the media properties
    char *redirect = " | grep 'Overall bit rate                         : ' > info.txt";//Writing down the output (bit rate) into a file
    char command[256];
    strcpy(command, command1);
    strcat(command, fName);
    strcat(command, redirect);
    system(command);
        // execl(params[0], params[1], params[2],params[3],NULL);
    wait(NULL);
    FILE* file = fopen("info.txt", "r");
    char *strng = "Overall bit rate                         : ";
    char buffer1[256];
    memset(buffer1, '\0', sizeof(buffer1));//setting buffer to 0
    char *s;
    fgets(buffer1, sizeof(buffer1), file);
    char* p = strstr(buffer1,"Kbps\n");//string representing bit rate
    memset(p, '\0', strlen(p));//setting string to 0

    s = strstr(buffer1,strng);
    s = s+strlen(strng);
    
    char buffer2[256];
    memset(buffer2, '\0', 256);

    int i=0,j=0;
    for(i=0;i<strlen(s)+1;i++){
        if(s[i] == ' ')
            continue;
        buffer2[j++] = s[i];
    }

    int b_r = atoi(buffer2);//char array to int conversion
    return b_r;
}
//defining the function for calculation of bit rate
void b_r_calculate(char names[][BUFFER_SIZE_SMALL], int bitRate[], int sCount)
{
    for (int i = 0; i < sCount; i++)
    {
        bitRate[i] = getBitRate(names[i]);
    }
}

//Defining function for sending structures to stations
void *initializeSongServer(void *arg)
{
    //Structure object initialised
    struct sockaddr_in sin;
    int len;
    int socket1, new_socket;
    char strng[INET_ADDRSTRLEN];
    //Server IP initialised and declared
    char *serverIP;
    if (argC > 1)
        serverIP = argV[1];
    /* build address data structure */
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET; //assigning family
    sin.sin_addr.s_addr = inet_addr(serverIP);//assigning server address
    sin.sin_port = htons(SERVER_PORT);//assigning server port
    /* setup passive open */
    if ((socket1 = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("unsuccessful socket creation");//generating error on failure of socket
        exit(1);//program is terminated if socket is not created
    }
    
    inet_ntop(AF_INET, &(sin.sin_addr), strng, INET_ADDRSTRLEN);//converting address from network format to presentation format
    printf("Server is using address %s and port %d.\n", strng, SERVER_PORT);
    //Binding server to the address
    if ((bind(socket1, (struct sockaddr *)&sin, sizeof(sin))) < 0)
    {
        perror("error while Binding");//generating error if bind not done
        exit(1);//termination on failure of bind
    }
    else
        printf("successful binding\n");
    
    listen(socket1, 5);//call for listen where pending request can be atmost 5
    //Sending structures infinitely to client
    while (1)
    {
        //Accepting the connection request from client
        if ((new_socket = accept(socket1, (struct sockaddr *)&sin, (unsigned int *)&len)) < 0)
        {
            perror("failed to accept");//Generating error if accept command fails
            exit(1);//terminating on failure of accept call
        }
        printf("Client and server connected! Starting to send structures\n");
        uint32_t nos = 10;//initialising no of stations
        nos = htonl(nos);//sending int requires this conversion
        send(new_socket, &nos, sizeof(uint32_t), 0);//no of stations(int) is sent
        
        for (int i = 0; i < NO_OF_STATIONS; i++)//sending the station info for all the stations specified
        {
            send(new_socket, &songs_store[i], sizeof(song_info), 0);
        }
    }
}
//Defining function fill stations
void songStoreInfo()
{
    song_info stat_info1;
    song_no_store_path stat_id_path;
    
    //Initialising station1 info into structure variables
    stat_info1.song_no = 1;
    stat_info1.song_store_size = htonl(strlen("Song 1"));
    strcpy(stat_info1.song_store_name, "Song 1");
    stat_info1.data_port = htons(8200);
    stat_id_path.port = 8200;
    stat_id_path.no = 1;
    strcpy(stat_id_path.path, "./Song_1/");
    
    //Copying station1 info and path
    memcpy(&songs_store[0], &stat_info1, sizeof(song_info));
    memcpy(&songnostorepath[0], &stat_id_path, sizeof(song_no_store_path));
    
    //Clearing out station1 info and path
    bzero(&stat_info1, sizeof(song_info));
    bzero(&stat_id_path, sizeof(song_no_store_path));
    
    //Initialising station2 info into structure variables
    stat_info1.song_no = 2;
    stat_info1.song_store_size = htonl(strlen("Song 2"));
    strcpy(stat_info1.song_store_name, "Song 2");
    stat_info1.data_port = htons(8201);
    stat_id_path.port = 8201;
    stat_id_path.no = 2;
    strcpy(stat_id_path.path, "./Song_2/");
    
    //Copying station2 info and path
    memcpy(&songs_store[1], &stat_info1, sizeof(song_info));
    memcpy(&songnostorepath[1], &stat_id_path, sizeof(song_no_store_path));
    
    //Clearing out station2 info and path
    bzero(&stat_info1, sizeof(song_info));
    bzero(&stat_id_path, sizeof(song_no_store_path));
    
    //Initialising station3 info into structure variables
    stat_info1.song_no = 3;
    stat_info1.song_store_size = htonl(strlen("Song 3"));
    strcpy(stat_info1.song_store_name, "Song 3");
    stat_info1.data_port = htons(8202);
    stat_id_path.port = 8202;
    stat_id_path.no = 3;
    strcpy(stat_id_path.path, "./Song_3/");
    
    //Copying station3 info and path
    memcpy(&songs_store[2], &stat_info1, sizeof(song_info));
    memcpy(&songnostorepath[2], &stat_id_path, sizeof(song_no_store_path));

    //Clearing out station3 info and path
    bzero(&stat_info1, sizeof(song_info));
    bzero(&stat_id_path, sizeof(song_no_store_path));
    

   //Initialising station4 info into structure variables
    stat_info1.song_no = 4;
    stat_info1.song_store_size = htonl(strlen("Song 4"));
    strcpy(stat_info1.song_store_name, "Song 4");
    stat_info1.data_port = htons(8203);
    stat_id_path.port = 8203;
    stat_id_path.no = 4;
    strcpy(stat_id_path.path, "./Song_4/");
    
    //Copying station4 info and path
    memcpy(&songs_store[3], &stat_info1, sizeof(song_info));
    memcpy(&songnostorepath[3], &stat_id_path, sizeof(song_no_store_path));

    //Clearing out station4 info and path
    bzero(&stat_info1, sizeof(song_info));
    bzero(&stat_id_path, sizeof(song_no_store_path));
   
   //Initialising station5 info into structure variables
    stat_info1.song_no = 5;
    stat_info1.song_store_size = htonl(strlen("Song 5"));
    strcpy(stat_info1.song_store_name, "Song 5");
    stat_info1.data_port = htons(8204);
    stat_id_path.port = 8204;
    stat_id_path.no = 5;
    strcpy(stat_id_path.path, "./Song_5/");
    
    //Copying station5 info and path
    memcpy(&songs_store[4], &stat_info1, sizeof(song_info));
    memcpy(&songnostorepath[4], &stat_id_path, sizeof(song_no_store_path));

    //Clearing out station5 info and path
    bzero(&stat_info1, sizeof(song_info));
    bzero(&stat_id_path, sizeof(song_no_store_path));
    
    //Initialising station6 info into structure variables
    stat_info1.song_no = 6;
    stat_info1.song_store_size = htonl(strlen("Song 6"));
    strcpy(stat_info1.song_store_name, "Song 6");
    stat_info1.data_port = htons(8205);
    stat_id_path.port = 8205;
    stat_id_path.no = 6;
    strcpy(stat_id_path.path, "./Song_6/");
    
    //Copying station6 info and path
    memcpy(&songs_store[5], &stat_info1, sizeof(song_info));
    memcpy(&songnostorepath[5], &stat_id_path, sizeof(song_no_store_path));

    //Clearing out station6 info and path
    bzero(&stat_info1, sizeof(song_info));
    bzero(&stat_id_path, sizeof(song_no_store_path));
   
    //Initialising station7 info into structure variables
    stat_info1.song_no = 7;
    stat_info1.song_store_size = htonl(strlen("Song 7"));
    strcpy(stat_info1.song_store_name, "Song 7");
    stat_info1.data_port = htons(8206);
    stat_id_path.port = 8206;
    stat_id_path.no = 7;
    strcpy(stat_id_path.path, "./Song_7/");
    
    //Copying station7 info and path
    memcpy(&songs_store[6], &stat_info1, sizeof(song_info));
    memcpy(&songnostorepath[6], &stat_id_path, sizeof(song_no_store_path));

    //Clearing out station7 info and path
    bzero(&stat_info1, sizeof(song_info));
    bzero(&stat_id_path, sizeof(song_no_store_path));
   
    //Initialising station8 info into structure variables
    stat_info1.song_no = 8;
    stat_info1.song_store_size = htonl(strlen("Song 8"));
    strcpy(stat_info1.song_store_name, "Song 8");
    stat_info1.data_port = htons(8207);
    stat_id_path.port = 8207;
    stat_id_path.no = 8;
    strcpy(stat_id_path.path, "./Song_8/");
    
    //Copying station8 info and path
    memcpy(&songs_store[7], &stat_info1, sizeof(song_info));
    memcpy(&songnostorepath[7], &stat_id_path, sizeof(song_no_store_path));

    //Clearing out station8 info and path
    bzero(&stat_info1, sizeof(song_info));
    bzero(&stat_id_path, sizeof(song_no_store_path));
   
    //Initialising station9 info into structure variables
    stat_info1.song_no = 9;
    stat_info1.song_store_size = htonl(strlen("Song 9"));
    strcpy(stat_info1.song_store_name, "Song 9");
    stat_info1.data_port = htons(8208);
    stat_id_path.port = 8208;
    stat_id_path.no = 9;
    strcpy(stat_id_path.path, "./Song_9/");
    
    //Copying station9 info and path
    memcpy(&songs_store[8], &stat_info1, sizeof(song_info));
    memcpy(&songnostorepath[8], &stat_id_path, sizeof(song_no_store_path));

    //Clearing out station9 info and path
    bzero(&stat_info1, sizeof(song_info));
    bzero(&stat_id_path, sizeof(song_no_store_path));
 
    //Initialising station10 info into structure variables
    stat_info1.song_no = 10;
    stat_info1.song_store_size = htonl(strlen("Song 10"));
    strcpy(stat_info1.song_store_name, "Song 10");
    stat_info1.data_port = htons(8209);
    stat_id_path.port = 8209;
    stat_id_path.no = 10;
    strcpy(stat_id_path.path, "./Song_10/");
    
    //Copying station10 info and path
    memcpy(&songs_store[9], &stat_info1, sizeof(song_info));
    memcpy(&songnostorepath[9], &stat_id_path, sizeof(song_no_store_path));

    //Clearing out station10 info and path
    bzero(&stat_info1, sizeof(song_info));
    bzero(&stat_id_path, sizeof(song_no_store_path));
     
}
//Function for starting / setting up station
void *startStation(void *arg)
{
    //Parsing directory and opening songs
    song_no_store_path *stat_id_path = (song_no_store_path *)arg;
    DIR *dir;
    struct dirent *entry;
    int sCount = 0;
    //path for song file
    printf("Path:-- %s\n", stat_id_path->path);
    //searching in the directory and then reading all the songs
    if ((dir = opendir(stat_id_path->path)) != NULL)
    {
        //Reading all the files with extension mp3 and then collecting them
        while ((entry = readdir(dir)) != NULL)
        {
            if (strstr(entry->d_name, ".mp3") != NULL)
                ++sCount;//counter for songs with mp3 extension
        }
        closedir(dir);//closing directory
    }
    else
    {
        /* could not open file in the directory */
        perror("Could not find file in the directory");
        return 0;
    }
    
      
    char songs[sCount][BUFFER_SIZE_SMALL];    //Declaring 2D array for path of the songs and their size
    char sNames[sCount][BUFFER_SIZE_SMALL];  //Declaring 2D array for names of the songs and their size
    
    FILE *songFiles[sCount];
    int bitRates[sCount];
    
    for (int i = 0; i < sCount; i++)
    {
        memset(songs[i], '\0', BUFFER_SIZE_SMALL);
        strcpy(songs[i], stat_id_path->path);     //storing path of the songs
    }
    
    int cur = 0;
    if ((dir = opendir(stat_id_path->path)) != NULL)       //Checks for station directory is not empty
    {
        while ((entry = readdir(dir)) != NULL)       //proceeds to read the songs if directory is not null
        {
            if (strstr(entry->d_name, ".mp3") != NULL)
            {
                memcpy(&(songs[cur][strlen(stat_id_path->path)]), entry->d_name, strlen(entry->d_name) + 1);
                strcpy((sNames[cur]), entry->d_name);    //stores names of the available songs
                 
                songFiles[cur] = fopen(songs[cur], "rb");  //Opening the file
                if (songFiles[cur] == NULL)
                {
                    perror("No song file present in the directory");  //Displaying error for no song files
                    exit(1);
                }
                
                cur++;   //incrementing value to check for all the files in the directory
            }
        }
        closedir(dir);  //closing the current directory
    }
    
    //Creating Song_Info Structures
    song_play_info sInfo[sCount];
    
    for (int i = 0; i < sCount; i++)
        bzero(&sInfo[i], sizeof(song_play_info));   //zeros out the struct for information of songs
    
    for (int i = 0; i <= sCount; i++)
    {
        initSongPlayInfo(&sInfo[i]);   //calling initSongPlayInfo for the number of songs
        printf("song info : %hu p = %p\n", (unsigned short)sInfo[i].type, &sInfo[i].type);
    }
    
    for (int i = 0; i < sCount; i++)
    {
        //Fetching size and name of the song for the current song
        sInfo[i].song_play_name_size = (uint8_t)strlen(sNames[i]) + 1;
        printf("%d", sInfo[i].song_play_name_size);
        strcpy((sInfo[i].song_play_name), sNames[i]);
        
        //Fetching size and name of the next song
        sInfo[i].next_song_play_name_size = (uint8_t)strlen(sNames[(i + 1) % sCount]) + 1;
        strcpy((sInfo[i].next_song_play_name), sNames[(i + 1) % sCount]);
    }
    
    //Displaying information about songs
    for (int i = 0; i < sCount; i++)
    {
        printf("%s\n", songs[i]);
        printf("Song info : %hu p = %p\n", (unsigned short)sInfo[i].type, &sInfo[i].type);
    }
    
    int socket1;            //socket variable
    struct sockaddr_in sin; //object that refers to sockaddr structure
    int len;
    char buffer1[BUFFER_SIZE_SMALL];
    char *multi_cast_address; /* multicast address */
    
    struct timespec t_s;
    t_s.tv_sec = 0;
    t_s.tv_nsec = 20000000L;
    //Multicast address initialisation
    multi_cast_address = "192.168.43.69";
    
    //socket creation for UDP multicase
    if ((socket1 = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("unsuccessful socket creation");//Error if socket is not created
        exit(1);
    }
    
    //build address data structure
    memset((char *)&sin, 0, sizeof(sin));//setting address variable to 0 using memset
    sin.sin_family = AF_INET;//assigning family
    sin.sin_addr.s_addr = inet_addr(multi_cast_address);//assigning multicast address
    sin.sin_port = htons(stat_id_path->port);//assigning port number
    printf("\nplaying song id : %d!\n\n", stat_id_path->no);
    
    /* Send multicast messages */
    memset(buffer1, 0, sizeof(buffer1));
    
    int current_song = -1;
    while (1)
    {
        //Choosing songs one by one
        current_song = (current_song + 1) % sCount;
        //Pointer for song
        FILE *song = songFiles[current_song];
        //In case when song is not found
        if (song == NULL)
            printf("Song not found!!\n");
        //Printing song number and song name
        printf("Curent Song number = %d current Song name= %p\n", current_song, song);
        rewind(song);//Setting pointer to the beginning of file
        
        int size = BUFFER_SIZE_SMALL;
        int count = 0;
        //Printing structure which is sent
        printf("Sending Structure : current Song number= %d. Song_Info->type = %hu p = %p\n", current_song, (unsigned short)sInfo[current_song].type, &sInfo[current_song].type);
        
        if ((len = sendto(socket1, &sInfo[current_song], sizeof(song_play_info), 0, (struct sockaddr *)&sin, sizeof(sin))) == -1)
        {
            perror("Server sending failed");
            exit(1);
        }
        // calculating sleep time in accordance with bit rate
            float bitrate = bitRates[current_song];
            idle_sleep = ((BUFFER_SIZE_SMALL * 8) / bitrate) * 500000000;
            
            // if sleep time is less than zero, assigning it to the default value specified
            if (idle_sleep < 0)
                idle_sleep = t_s.tv_nsec;
            
            // if sleep time is greater than zero, assigning it to the idle_sleep
            if (t_s.tv_nsec > idle_sleep)
                t_s.tv_nsec = idle_sleep;
            
            while (!(size < BUFFER_SIZE_SMALL))
            {
                // Sending the contents of song
                size = fread(buffer1, 1, sizeof(buffer1), song);
                
                if ((len = sendto(socket1, buffer1, size, 0, (struct sockaddr *)&sin, sizeof(sin))) == -1)
                {
                    perror("server: sendto");
                    exit(1);
                }
                if (len != size)
                {
                    printf("ERROR!!");
                    exit(0);
                }

                // Delaying the in between packet time in order to reduce packet loss at the client side
            // Delaying it with the time assigned to idle_sleep
                nanosleep(&t_s, NULL);
                memset(buffer1, 0, sizeof(buffer1));  //Setting buffer to 0
            }
        }
        //closing the socket
        close(socket1);
}


int main(int argc, char *argv[])
{
    //Assigning command line arguments to variables
    argC = argc;
    argV = argv;
    //Initializing Stations
    songStoreInfo();
    
    // Starting TCP Server
    //Declaring and creating pthread for starting TCP connection
    pthread_t tTCPid;
    pthread_create(&tTCPid, NULL, initializeSongServer, NULL);
    //Starting all stations
    for (int i = 0; i < NO_OF_STATIONS; i++)
    {
        //creating thread for each station
        pthread_create(&songstorethread[i], NULL, startStation, &songnostorepath[i]);
    }
    //waits for the thread tTCPid to terminate
    pthread_join(tTCPid, NULL);
    return 0;
}
