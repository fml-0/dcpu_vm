#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "file.h"
#include <dcpu/devices/m35fd.hpp>
#include <log.hpp>


int main(int argc, char** argv)
{
  if (argc < 3)
  {
    LOG_ERROR << "no input arguments";
    LOG << std::string("Usage: ") + std::string(argv[0]) + " input output";
    return 0xdead;
  }
  FILE* in = fopen(argv[1],"rb");
  if (!in)
  {
    LOG_ERROR << std::string(argv[1]) + " : cannot open the file !";
  }
  int in_size = fsize(in);
  char* buffer_in = (char*) malloc(in_size + 2*cpu::m35fd::SECTOR_SIZE);
  fread(buffer_in,1,in_size,in);
  fclose(in);
  
  unsigned char master_boot_record[512] = {0};
  unsigned end_sector = in_size/cpu::m35fd::SECTOR_SIZE + 1;
  
  //Bootable master_boot_record magic
  master_boot_record[510]=0x55;
  master_boot_record[511]=0xAA;
  
  if (in_size <= 440) //Use MBR to put our program
  {
    memcpy(master_boot_record,buffer_in,in_size);
    LOG << "Use MBR to boot";
  }
  else
  {
    LOG << "Use mrboot signature to boot";
    //MrBoot Special Magic
    master_boot_record[440] = 0xAE; 
    master_boot_record[441] = 0xFB;
    //0xFBAE : Floppy Bootable And Executable
    
    
    master_boot_record[442] = 0x1; //begin program sector
    master_boot_record[443] = end_sector;//End program sector...
    
    //Reserve partition 0 for our program
    master_boot_record[446] = 0x8; //Active
    master_boot_record[446+2] = 0x1;
    master_boot_record[446+4] = 0xFE; //partition type
    master_boot_record[446+6] = end_sector; 
    master_boot_record[446+0xc] = end_sector - 1;    
  }
  
  
  
  cpu::m35fd::M35_Floppy floppy(argv[2]);
  floppy.writeToFile(0,(char*)master_boot_record);
  if (in_size > 440) 
  {
    for (unsigned i=1;i<=end_sector;i++)
    {
      floppy.writeToFile(i,&(buffer_in[cpu::m35fd::SECTOR_SIZE*(i-1)]));
    }
  }
  free(buffer_in);
  return 0;
}