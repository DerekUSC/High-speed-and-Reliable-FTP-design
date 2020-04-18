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
#include<time.h>

#define SERVER_PORT1 33333
#define SERVER_PORT2 8000
#define CLIENT_PORT 12345 
#define BUFFER_SIZE 1500 
#define FILE_NAME_MAX_SIZE 512

/* 包头 */
typedef struct 
{ 
  long int id; 
  int buf_size; 
  unsigned int  crc32val;   //每一个buffer的crc32值
  int errorflag;
}PackInfo; 
  
/* 接收包 */
struct RecvPack 
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
int main() 
{
	// count time
  clock_t start, end;
  double T;

  long int id = 1; 
  unsigned int crc32tmp;
  
  start = clock();

  /* 服务端 address 1 port 33333
	 send Acknum
  */
  struct sockaddr_in server_addr1; 
  bzero(&server_addr1, sizeof(server_addr1)); 
  server_addr1.sin_family = AF_INET; 
  server_addr1.sin_addr.s_addr = inet_addr("192.168.1.78"); 
  server_addr1.sin_port = htons(SERVER_PORT1); 
  socklen_t server_addr_length1 = sizeof(server_addr1);

  /* 服务端 address 2 port 8000
	 send filename
	 recv packet_size
	 recv data
  */
  struct sockaddr_in server_addr2; 
  bzero(&server_addr2, sizeof(server_addr2)); 
  server_addr2.sin_family = AF_INET; 
  server_addr2.sin_addr.s_addr = inet_addr("192.168.1.78"); 
  server_addr2.sin_port = htons(SERVER_PORT2); 
  socklen_t server_addr_length2 = sizeof(server_addr2);

  /* 创建socket port 33333 */
  int client_socket_fd1 = socket(AF_INET, SOCK_DGRAM, 0); 
  if(client_socket_fd1 < 0) 
  { 
    printf("Create Socket Failed:"); 
    exit(1); 
  } 

   /* 创建socket port 8000 */
  int client_socket_fd2 = socket(AF_INET, SOCK_DGRAM, 0); 
  if(client_socket_fd2 < 0) 
  { 
    printf("Create Socket Failed:"); 
    exit(1); 
  }

       //crc32
  init_crc_table();

    // /* 输入文件名到缓冲区 */
  char file_name[FILE_NAME_MAX_SIZE+1]; 
  bzero(file_name, FILE_NAME_MAX_SIZE+1); 
  printf("Please Input File Name On Server: "); 
  scanf("%s", file_name); 
  
  char buffer[BUFFER_SIZE]; 
  bzero(buffer, BUFFER_SIZE); 
  strncpy(buffer, file_name, strlen(file_name)>BUFFER_SIZE?BUFFER_SIZE:strlen(file_name)); 
  
  /* 发送文件名 */
  if(sendto(client_socket_fd2, buffer, BUFFER_SIZE,0,(struct sockaddr*)&server_addr2,server_addr_length2) < 0) 
  { 
    printf("Send File Name Failed:"); 
    exit(1); 
  }

    /* 打开文件，准备写入 */
  FILE *fp1 = fopen(file_name, "w");
  fclose(fp1);
  FILE *fp = fopen(file_name, "rb+"); 
  if(NULL == fp) 
  { 
    printf("File:\t%s Can Not Open To Write\n", file_name);  
    exit(1); 
  }

    /* 从服务器接收数据，并写入文件 */
  int len = 0;
  int count = 0;
  int packet_size = 0;
  int sign = 0;
  long int pos = 0;
  recvfrom(client_socket_fd2, &packet_size, sizeof(packet_size), 0, (struct sockaddr*)&server_addr2,&server_addr_length2);
  printf("packet size: %d\n", packet_size);

  while(1) {

  	PackInfo pack_info; //定义确认包变量
  
    if((len = recvfrom(client_socket_fd2, (char*)&data, sizeof(data), 0, (struct sockaddr*)&server_addr2,&server_addr_length2)) > 0) 
    { 
      printf("count = %d\n",count);
      crc32tmp = crc32(crc,data.buf,sizeof(data));
      if (count > packet_size) {
        
        sendto(client_socket_fd1, &sign, sizeof(sign),0,(struct sockaddr*)&server_addr1,server_addr_length1);
        break;
      }
      
      if(count <= packet_size && data.head.crc32val == crc32tmp) 
      {
         count++;
         // printf("count %d\n", count);
      	 pack_info.id = data.head.id;

      	 pack_info.buf_size = data.head.buf_size;

  	 	/* 发送数据包确认信息 */
        if(sendto(client_socket_fd1, &pack_info.id, sizeof(pack_info.id), 0, (struct sockaddr*)&server_addr1, server_addr_length1) < 0) 
        { 
          printf("Send confirm information failed!"); 
        }
        // printf("number: %ld\n", pack_info.id);
		/* 写入文件 */
        pos = (pack_info.id - 1) * BUFFER_SIZE;
        // printf("position: %ld\n", pos);
        if(-1 == (fseek(fp, pos, SEEK_SET)))
            printf("seek error\n");

        if(fwrite(data.buf, sizeof(char), data.head.buf_size, fp) < data.head.buf_size) 
        { 
          printf("File:\t%s Write Failed\n", file_name); 
          //break; 
        }  
	  }
  	}
  }

  printf("Receive File:\t%s From Server IP Successful!\n", file_name); 
  fclose(fp); 
  close(client_socket_fd1);
  close(client_socket_fd2);
  end = clock();
  T =  ((double) (end - start)) / CLOCKS_PER_SEC;
  printf("time: %f\n", T);
  return 0;
}