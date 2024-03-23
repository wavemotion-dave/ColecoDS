/** EMULib Emulation Library *********************************/
/**                                                         **/
/**                        FDIDisk.c                        **/
/**                                                         **/
/** This file contains routines to load, save, and access   **/
/** disk images in various formats. The internal format is  **/
/** always .FDI. See FDIDisk.h for declarations.            **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 2007-2021                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#include "FDIDisk.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "colecomngt.h"
#include "printf.h"

#define IMAGE_SIZE(Fmt) \
  (Formats[Fmt].Sides*Formats[Fmt].Tracks*    \
   Formats[Fmt].Sectors*Formats[Fmt].SecSize)

#define FDI_SIDES(P)      ((P)[6]+((int)((P)[7])<<8))
#define FDI_TRACKS(P)     ((P)[4]+((int)((P)[5])<<8))
#define FDI_DIR(P)        ((P)+(P)[12]+((int)((P)[13])<<8)+14)
#define FDI_DATA(P)       ((P)+(P)[10]+((int)((P)[11])<<8))
#define FDI_INFO(P)       ((P)+(P)[8]+((int)((P)[9])<<8))
#define FDI_SECTORS(T)    ((T)[6])
#define FDI_TRACK(P,T)    (FDI_DATA(P)+(T)[0]+((int)((T)[1])<<8)+((int)((T)[2])<<16)+((int)((T)[3])<<24))
#define FDI_SECSIZE(S)    (SecSizes[(S)[3]<=4? (S)[3]:4])
#define FDI_SECTOR(P,T,S) (FDI_TRACK(P,T)+(S)[5]+((int)((S)[6])<<8))

static const struct { int Sides,Tracks,Sectors,SecSize; } Formats[] =
{
  { 2,  80, 16, 256 }, /* Dummy format */
  { 2,  80, 10, 512 }, /* FMT_FDI       - Generic FDI image */
  { 1,  40, 8,  512 }, /* FMT_ADMDSK    - Coleco Adam disk 160K */
  { 1,  32, 16, 512 }, /* FMT_DDP       - Coleco Adam tape 256K */
  { 2,  40, 8,  512 }, /* FMT_ADMDSK320 - Coleco Adam disk 320K */
};

static const int SecSizes[] =
{ 128,256,512,1024,4096,0 };

static const char FDIDiskLabel[] =
"Disk image created by EMULib (C)Marat Fayzullin";

extern byte ROM_Memory[];

/** InitFDI() ************************************************/
/** Clear all data structure fields.                        **/
/*************************************************************/
void InitFDI(FDIDisk *D)
{
  D->Format   = 0;
  D->Data     = 0;
  D->DataSize = 0;
  D->Sides    = 0;
  D->Tracks   = 0;
  D->Sectors  = 0;
  D->SecSize  = 0;
}

/** EjectFDI() ***********************************************/
/** Eject disk image. Free all allocated memory.            **/
/*************************************************************/
void EjectFDI(FDIDisk *D)
{
  InitFDI(D);
}

/** NewFDI() *************************************************/
/** Allocate memory and create new .FDI disk image of given **/
/** dimensions. Returns disk data pointer on success, 0 on  **/
/** failure.                                                **/
/*************************************************************/
byte *NewFDI(FDIDisk *D,int Sides,int Tracks,int Sectors,int SecSize)
{
  byte *P,*DDir;
  int I,J,K,L,N;

  /* Find sector size code */
  for(L=0;SecSizes[L]&&(SecSizes[L]!=SecSize);++L);
  if(!SecSizes[L]) return(0);

  /* Allocate memory */
  K = Sides*Tracks*Sectors*SecSize+sizeof(FDIDiskLabel);
  I = Sides*Tracks*(Sectors+1)*7+14;
    
  if ((I+K) > MAX_FDID_SIZE) return 0;   // Too big...
  
  // ----------------------------------------------------------
  // Reuse the huge ROM_Memory[] array as it has no other
  // use at this point and it's more efficient than allocating
  // another big chunk of memory off the heap!  We have space
  // for two 320K Adam .DSK files (or .ddp files). The layout
  // is this:
  // ROM_Memory+0K   is for the read-in buffer from the SD card.
  // ROM_Memory+330K is the converted FDID image for DISK0
  // ROM_Memory+660K is the converted FDID image for DISK1
  // ----------------------------------------------------------
  int offset = ((D == &Disks[0]) ? MAX_FDID_SIZE : (2*MAX_FDID_SIZE));
  P = (byte*)ROM_Memory+offset;   // index into the big buffer depending on whether we are DISK0 or DISK1
  memset(P,0x00,I+K);             // Clear out the disk buffer - we'll assemble the FDID image below
    
  /* Eject previous disk image */
  EjectFDI(D);

  /* Set disk dimensions according to format */
  D->Format   = FMT_FDI;
  D->Data     = P;
  D->DataSize = I+K;
  D->Sides    = Sides;
  D->Tracks   = Tracks;
  D->Sectors  = Sectors;
  D->SecSize  = SecSize;

  /* .FDI magic number */
  memcpy(P,"FDI",3);
  /* Disk description */
  memcpy(P+I,FDIDiskLabel,sizeof(FDIDiskLabel));
  /* Write protection (1=ON) */
  P[3]  = 0;
  P[4]  = Tracks&0xFF;
  P[5]  = Tracks>>8;
  P[6]  = Sides&0xFF;
  P[7]  = Sides>>8;
  /* Disk description offset */
  P[8]  = I&0xFF;
  P[9]  = I>>8;
  I    += sizeof(FDIDiskLabel);
  /* Sector data offset */
  P[10] = I&0xFF;
  P[11] = I>>8;
  /* Track directory offset */
  P[12] = 0;
  P[13] = 0;

  /* Create track directory */
  for(J=K=0,DDir=P+14;J<Sides*Tracks;++J,K+=Sectors*SecSize)
  {
    /* Create track entry */
    DDir[0] = K&0xFF;
    DDir[1] = (K>>8)&0xFF;
    DDir[2] = (K>>16)&0xFF;
    DDir[3] = (K>>24)&0xFF;
    /* Reserved bytes */
    DDir[4] = 0;
    DDir[5] = 0;
    DDir[6] = Sectors;
    /* For all sectors on a track... */
    for(I=N=0,DDir+=7;I<Sectors;++I,DDir+=7,N+=SecSize)
    {
      /* Create sector entry */
      DDir[0] = J/Sides;
      DDir[1] = J%Sides;
      DDir[2] = I+1;
      DDir[3] = L;
      /* CRC marks and "deleted" bit (D00CCCCC) */
      DDir[4] = (1<<L);
      DDir[5] = N&0xFF;
      DDir[6] = N>>8;
    }
  }

  /* Done */
  return(FDI_DATA(P));
}


/** LoadFDI() ************************************************/
/** Load a disk image from a given file, in a given format  **/
/** (see FMT_* #defines). Guess format from the file name   **/
/** when Format=FMT_AUTO. Returns format ID on success or   **/
/** 0 on failure. When FileName=0, ejects the disk freeing  **/
/** memory and returns 0.                                   **/
/*************************************************************/
int LoadFDI(FDIDisk *D,const char *FileName,int Format)
{
  byte *P;
  int J;
  FILE *F;

  /* If just ejecting a disk, drop out */
  if(!FileName) { EjectFDI(D);return(0); }

  /* Open file and find its size */
  if(!(F=fopen(FileName,"rb"))) return(0);
  if(fseek(F,0,SEEK_END)<0) { fclose(F);return(0); }
  if((J=ftell(F))<=0)       { fclose(F);return(0); }
  rewind(F);

  switch(Format)
  {
    case FMT_ADMDSK: /* If Coleco Adam .DSK format... */
    case FMT_DDP:    /* If Coleco Adam .DDP format... */
    case FMT_ADMDSK320:
      /* Create a new disk image */
      P = FormatFDI(D,Format);
      if(!P) { fclose(F);return(0); }
      /* Read disk image file (ignore short reads!) */
      fread(P,1,IMAGE_SIZE(Format),F);
      /* Done */
      P = D->Data;
      break;

    default:
      /* Format not recognized */
      return(0);
  }

  /* Done */
  fclose(F);
  D->Data   = P;
  D->Format = Format;
  return(Format);
}

/** SaveDSKData() ********************************************/
/** Save uniform disk data, truncating or adding zeros as   **/
/** needed. Returns FDI_SAVE_OK on success, FDI_SAVE_PADDED **/
/** if any sectors were padded, FDI_SAVE_TRUNCATED if any   **/
/** sectors were truncated, FDI_SAVE_FAILED if failed.      **/
/*************************************************************/
static int SaveDSKData(FDIDisk *D,FILE *F,int Sides,int Tracks,int Sectors,int SecSize)
{
  int J,I,K,Result;

  Result = FDI_SAVE_OK;

  /* Scan through all tracks, sides, sectors */
  for(J=0;J<Tracks;++J)
    for(I=0;I<Sides;++I)
      for(K=0;K<Sectors;++K)
      {
        /* Seek to sector and determine actual sector size */
        byte *P = SeekFDI(D,I,J,I,J,K+1);
        int   L = D->SecSize<SecSize? D->SecSize:SecSize;
        /* Write sector */
        if(!P||!L||(fwrite(P,1,L,F)!=L)) return(FDI_SAVE_FAILED);
        /* Pad sector to SecSize, if needed */
        if((SecSize>L)&&fseek(F,SecSize-L,SEEK_CUR)) return(FDI_SAVE_FAILED);
        /* Update result */
        L = SecSize>L? FDI_SAVE_PADDED:SecSize<L? FDI_SAVE_TRUNCATED:FDI_SAVE_OK;
        if(L<Result) Result=L;
      }

  /* Done */
  return(Result);
}

/** SaveFDI() ************************************************/
/** Save a disk image to a given file, in a given format    **/
/** (see FMT_* #defines). Use the original format when      **/
/** when Format=FMT_AUTO. Returns FDI_SAVE_OK on success,   **/
/** FDI_SAVE_PADDED if any sectors were padded,             **/
/** FDI_SAVE_TRUNCATED if any sectors were truncated,       **/
/** FDI_SAVE_FAILED (0) if failed.                          **/
/*************************************************************/
int SaveFDI(FDIDisk *D,const char *FileName,int Format)
{
  int Result;
  FILE *F; 

  /* Must have a disk to save */
  if(!D->Data) return(0);

  /* Use original format if requested */
  if(!Format) Format=D->Format;

  /* Open file for writing */
  if(!(F=fopen(FileName,"wb"))) return(0);

  /* Assume success */
  Result = FDI_SAVE_OK;

  /* Depending on the format... */
  switch(Format)
  {
    case FMT_FDI:
      break;

    case FMT_ADMDSK:
    case FMT_DDP:
    case FMT_ADMDSK320:
      /* Check the number of tracks and sides */
      if((FDI_TRACKS(D->Data)!=Formats[Format].Tracks)||(FDI_SIDES(D->Data)!=Formats[Format].Sides))
      { fclose(F);unlink(FileName);return(0); }
      /* Write out the data, in tracks/sides/sectors order */
      Result = SaveDSKData(D,F,Formats[Format].Sides,Formats[Format].Tracks,Formats[Format].Sectors,Formats[Format].SecSize);
      if(!Result) { fclose(F);unlink(FileName);return(0); }
      break;

    default:
      /* Can't save this format for now */
      fclose(F);
      unlink(FileName);
      return(0);
  }

  /* Done */
  fclose(F);
  return(Result);
}

/** SeekFDI() ************************************************/
/** Seek to given side/track/sector. Returns sector address **/
/** on success or 0 on failure.                             **/
/*************************************************************/
byte *SeekFDI(FDIDisk *D,int Side,int Track,int SideID,int TrackID,int SectorID)
{
  byte *P,*T;
  int J,Deleted;

  /* Have to have disk mounted */
  if(!D||!D->Data) return(0);

  /* May need to search for deleted sectors */
  Deleted = (SectorID>=0) && (SectorID&SEEK_DELETED)? 0x80:0x00;
  if(Deleted) SectorID&=~SEEK_DELETED;

  switch(D->Format)
  {
    case FMT_FDI:
    case FMT_ADMDSK:
    case FMT_DDP:
    case FMT_ADMDSK320:
      /* Track directory */
      P = FDI_DIR(D->Data);
      /* Find current track entry */
      for(J=Track*D->Sides+Side%D->Sides;J;--J) P+=(FDI_SECTORS(P)+1)*7;
      /* Find sector entry */
      for(J=FDI_SECTORS(P),T=P+7;J;--J,T+=7)
        if((T[0]==TrackID)||(TrackID<0))
          if((T[1]==SideID)||(SideID<0))
            if(((T[2]==SectorID)&&((T[4]&0x80)==Deleted))||(SectorID<0))
              break;
      /* Fall out if not found */
      if(!J) return(0);
      /* FDI stores a header for each sector */
      D->Header[0] = T[0];
      D->Header[1] = T[1];
      D->Header[2] = T[2];
      D->Header[3] = T[3]<=3? T[3]:3;
      D->Header[4] = T[4];
      D->Header[5] = 0x00;
      /* FDI has variable sector numbers and sizes */
      D->Sectors   = FDI_SECTORS(P);
      D->SecSize   = FDI_SECSIZE(T);
      return(FDI_SECTOR(D->Data,P,T));
  }

  /* Unknown format */
  return(0);
}

/** LinearFDI() **********************************************/
/** Seek to given sector by its linear number. Returns      **/
/** sector address on success or 0 on failure.              **/
/*************************************************************/
byte *LinearFDI(FDIDisk *D,int SectorN)
{
  if(!D->Sectors || !D->Sides || (SectorN<0)) return(0);
  else
  {
    int Sector = SectorN % D->Sectors;
    int Track  = SectorN / D->Sectors / D->Sides;
    int Side   = (SectorN / D->Sectors) % D->Sides;
    return(SeekFDI(D,Side,Track,Side,Track,Sector+1));
  }
}

/** FormatFDI() ***********************************************/
/** Allocate memory and create new standard disk image for a **/
/** given format. Returns disk data pointer on success, 0 on **/
/** failure.                                                 **/
/**************************************************************/
byte *FormatFDI(FDIDisk *D,int Format)
{
  if((Format<0) || (Format>=sizeof(Formats)/sizeof(Formats[0]))) return(0);
  
  return(NewFDI(D,
    Formats[Format].Sides,
    Formats[Format].Tracks,
    Formats[Format].Sectors,
    Formats[Format].SecSize
  ));
}

