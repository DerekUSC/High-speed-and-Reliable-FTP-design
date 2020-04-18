#include<sys/types.h> 
#include<sys/socket.h> 
#include<unistd.h> 
#include<netinet/in.h> 
#include<arpa/inet.h> 
#include<stdio.h> 
#include<stdlib.h> 
#include<errno.h> 
#include<netdb.h> 
#include<stdarg.h> 
#include<string.h>
#include <pthread.h> 
// #include<queue>
  
#define BUFFER_SIZE 1500             //发送文件udp缓冲区大小
#define FILE_NAME_MAX_SIZE 512 
#define MAX_PACKAGE_NUM 700000


int ACKnums[MAX_PACKAGE_NUM];
int ResendNums[MAX_PACKAGE_NUM];
pthread_mutex_t m;

  
/* 包头 */
typedef struct
{ 
  long int id; 
  int buf_size;
  unsigned int  crc32val;   //每一个buffer的crc32值
  int errorflag;
}PackInfo; 
  
/* 接收包 */
struct SendPack 
{ 
  PackInfo head; 
  char buf[BUFFER_SIZE]; 
} data; 
 
 
//----------------------crc32----------------
static unsigned int crc_table[256];
static void init_crc_table(void);
static unsigned int crc32(unsigned int crc, unsigned char * buffer, unsigned int size);
/* 第一次传入的值需要固定,如果发送端使用该值计算crc校验码, 那么接收端也同样需要使用该值进行计算 */  
unsigned int crc = 0xffffffff;   
 
/* 
 * 初始化crc表,生成32位大小的crc表 
 * 也可以直接定义出crc表,直接查表, 
 * 但总共有256个,看着眼花,用生成的比较方便. 
 */  
static void init_crc_table(void)  
{  
	unsigned int c;  
	unsigned int i, j;  
 
	for (i = 0; i < 256; i++) 
	{  
		c = (unsigned int)i;  
 
		for (j = 0; j < 8; j++) 
		{  
			if (c & 1)  
				c = 0xedb88320L ^ (c >> 1);  
			else  
				c = c >> 1;  
		}  
 
		crc_table[i] = c;  
	}  
}  
 
 
/* 计算buffer的crc校验码 */  
static unsigned int crc32(unsigned int crc,unsigned char *buffer, unsigned int size)  
{  
	unsigned int i;  
 
	for (i = 0; i < size; i++) 
	{  
		crc = crc_table[(crc ^ buffer[i]) & 0xff] ^ (crc >> 8);  
	}  
 
	return crc ;  
} 
 
 
//主函数入口  
void *SendFunc() 
{ 
  int SERVER_PORT = 8000;
    /* 发送id */
  long int send_id = 0; 
  init_crc_table();
  /* 创建UDP套接口 */
  struct sockaddr_in server_addr; 
  bzero(&server_addr, sizeof(server_addr)); 
  server_addr.sin_family = AF_INET; 
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
  server_addr.sin_port = htons(SERVER_PORT); 
  
  /* 创建socket */
  int server_socket_fd = socket(AF_INET, SOCK_DGRAM, 0); 
  if(server_socket_fd == -1)                            
  { 
    printf("Create Socket Failed:"); 
    exit(1); 
  } 
  
  /* 绑定套接口 */
  if(-1 == (bind(server_socket_fd,(struct sockaddr*)&server_addr,sizeof(server_addr)))) 
  { 
    printf("Server Bind Failed:"); 
    exit(1); 
  } 
   
    /* 定义一个地址，用于捕获客户端地址 */
  struct sockaddr_in client_addr;
  socklen_t client_addr_length = sizeof(client_addr); 

  // /* 接收数据 */
  char buffer[BUFFER_SIZE]; 
  bzero(buffer, BUFFER_SIZE); 
  if(recvfrom(server_socket_fd, buffer, BUFFER_SIZE,0,(struct sockaddr*)&client_addr, &client_addr_length) == -1) 
  { 
    printf("Receive Data Failed:"); 
    exit(1); 
  } 
    /* 从buffer中拷贝出file_name */
  char file_name[FILE_NAME_MAX_SIZE+1]; 
  bzero(file_name,FILE_NAME_MAX_SIZE+1); 
  strncpy(file_name, buffer, strlen(buffer)>FILE_NAME_MAX_SIZE?FILE_NAME_MAX_SIZE:strlen(buffer)); 
  printf("%s\n", file_name); 

  /* 打开文件 */
  FILE *fp = fopen(file_name, "r"); 

  //get the size of file
  fseek(fp, 0, SEEK_END); // seek to end of file
  int size = ftell(fp); // get current file pointer
  fseek(fp, 0, SEEK_SET); // seek back to beginning of file
  int remainer = size%BUFFER_SIZE;
  int NUM_TOTAL_PACKET;
  if(remainer > 0) NUM_TOTAL_PACKET = size/BUFFER_SIZE + 1;
  else NUM_TOTAL_PACKET = size/BUFFER_SIZE;

  printf("the number of packet is: %d\n",NUM_TOTAL_PACKET);
  if(sendto(server_socket_fd, &NUM_TOTAL_PACKET, sizeof(NUM_TOTAL_PACKET), 0, (struct sockaddr*)&client_addr, client_addr_length) < 0) 
  { 
    perror("Error: "); 
  }

  if(NULL == fp) 
  { 
    printf("File:%s Not Found.\n", file_name); 
  } 
  else
  { 
    int len = 0; 
    /* 每读取一段数据，便将其发给客户端 */
    while(1) 
    { 
      
      PackInfo pack_info; 
      
      bzero((char *)&data,sizeof(data));  //ljh socket发送缓冲区清零

      
      // printf("receive_id=%d\n",receive_id);
      // printf("send_id=%ld\n",send_id);
      
      ++send_id; 
          
      if((len = fread(data.buf, sizeof(char), BUFFER_SIZE, fp)) > 0) 
      { 
        data.head.id = send_id; /* 发送id放进包头,用于标记顺序 */
        data.head.buf_size = len; /* 记录数据长度 */
        data.head.crc32val = crc32(crc,(unsigned char*)data.buf,sizeof(data));
        // printf("len =%d\n",len);
        // printf("data.head.crc32val=0x%x\n",data.head.crc32val);
        //printf("data.buf=%s\n",data.buf);
    
        if(sendto(server_socket_fd, (char*)&data, sizeof(data), 0, (struct sockaddr*)&client_addr, client_addr_length) < 0) 
        { 
          printf("SendFunc: Send File Failed\n"); 
          break; 
        }                       
      } 
      else
      { 
        break; 
      }  
    }//while(1)

    /* 关闭文件 */
    // fclose(fp); 
  }//else
  int sendid = 1;
  int collection_num = 1;
  while(1){
    PackInfo pack_info; 
    bzero((char *)&data,sizeof(data));  //ljh socket发送缓冲区清零
    pthread_mutex_lock(&m);
    while(ACKnums[sendid] == 1){
      sendid++;
    }
    pthread_mutex_unlock(&m);
    
    if(sendid == NUM_TOTAL_PACKET + 1) break;
    else{
      fseek(fp, (sendid-1)*BUFFER_SIZE, SEEK_SET);
      int len = fread(data.buf, sizeof(char), BUFFER_SIZE, fp);
      data.head.id = sendid;
      ResendNums[collection_num] = sendid;
      collection_num++;
      // printf("send id is equal to %d\n",send_id);
      data.head.buf_size = len; 
      data.head.crc32val = crc32(crc,(unsigned char*)data.buf,sizeof(data));
      if(sendto(server_socket_fd, (char*)&data, sizeof(data), 0, (struct sockaddr*)&client_addr, client_addr_length) < 0) 
      { 
        perror("Error: "); 
      }
      else{
        sendid++;
      }
    }//else
  }//internal while(1)
  // fclose(fp);
  //resend loop2: keep changing resend nums
  int newcollection_num = collection_num;
  printf("collection_num = %d\n",collection_num);
  // for(int i = 1;i <= collection_num;i++){
  //   printf("resend num = %d\n",ResendNums[i]);
  // }
  while(1){
    // printf("newcollection_num is equal to %d\n",newcollection_num);
    int count_newcollection_num = 1;
    for(int i = 1;i < newcollection_num;i++){
      pthread_mutex_lock(&m);
      while(ACKnums[ResendNums[i]] == 1){
        i++;
      }
      pthread_mutex_unlock(&m);
      
      PackInfo pack_info; 
      bzero((char *)&data,sizeof(data));
      int temp = ResendNums[i];
      fseek(fp, (ResendNums[i]-1)*BUFFER_SIZE, SEEK_SET);
      int len = fread(data.buf, sizeof(char), BUFFER_SIZE, fp);
      if(len == 0){
        printf("error: len = %d\n",len);
        exit(1);
      }
      // printf("send id is equal to %d\n",send_id);
      data.head.id = ResendNums[i];
      data.head.buf_size = len; 
      data.head.crc32val = crc32(crc,(unsigned char*)data.buf,sizeof(data));
      if(sendto(server_socket_fd, (char*)&data, sizeof(data), 0, (struct sockaddr*)&client_addr, client_addr_length) < 0) 
      { 
        perror("Error: "); 
      }
      ResendNums[count_newcollection_num] = temp;
      // printf("i = %d, count pointer = %d\n",i,count_newcollection_num);
      count_newcollection_num++;
    }
    newcollection_num = count_newcollection_num;
  }//the third while(1)
  fclose(fp);
  close(server_socket_fd); 
  return NULL; 
}

void *RecvFunc()
{

  int SERVER_PORT = 33333;
  struct sockaddr_in server_addr; 
  bzero(&server_addr, sizeof(server_addr)); 
  server_addr.sin_family = AF_INET; 
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
  server_addr.sin_port = htons(SERVER_PORT); 
  
  /* 创建socket */
  int server_socket_fd = socket(AF_INET, SOCK_DGRAM, 0); 
  if(server_socket_fd == -1) 
  { 
    printf("Create Socket Failed:"); 
    exit(1); 
  } 
  
  /* 绑定套接口 */
  if(-1 == (bind(server_socket_fd,(struct sockaddr*)&server_addr,sizeof(server_addr)))) 
  { 
    printf("Server Bind Failed:");
    exit(1); 
  } 
   
    /* 定义一个地址，用于捕获客户端地址 */
  struct sockaddr_in client_addr; 
  socklen_t client_addr_length = sizeof(client_addr); 


  /* 接收数据 */
  while(1){
    long int buffer = 0;
    if(recvfrom(server_socket_fd, &buffer, BUFFER_SIZE,0,(struct sockaddr*)&client_addr, &client_addr_length) == -1) 
    { 
      printf("Receive Data Failed:"); 
      exit(1);
    } 
    // printf("buffer is equal to %ld\n", buffer);
    pthread_mutex_lock(&m);
    ACKnums[buffer] = 1;
    pthread_mutex_unlock(&m);
    if(buffer == 0){
      printf("flag is set to 1, transmission finished\n");
      exit(1);
    }
  }
  return NULL;
}

int main(){
  // thread ThreadSend(SendFunc);
  // thread ThreadRecv(RecvFunc);

  // ThreadSend.join();
  // ThreadRecv.join();
  pthread_t ThreadSend;  
  pthread_t ThreadRecv;
  pthread_create(&ThreadSend, NULL, SendFunc, NULL);
  pthread_create(&ThreadRecv, NULL, RecvFunc, NULL);
  // pthread_join(ThreadRecv, NULL);
  pthread_join(ThreadRecv, NULL); 
  

  printf("Transfer Successful!\n"); 
  return 0;
}