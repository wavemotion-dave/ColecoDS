/** EMULib Emulation Library *********************************/
/**                                                         **/
/**                        FDIDisk.h                        **/
/**                                                         **/
/** This file declares routines to load, save, and access   **/
/** disk images in various formats. The internal format is  **/
/** always .FDI. See FDIDisk.c for the actual code.         **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 2007-2021                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#ifndef FDIDISK_H
#define FDIDISK_H
#ifdef __cplusplus
extern "C" {
#endif
                              /* SaveFDI() result:           */
#define FDI_SAVE_FAILED    0  /* Failed saving disk image    */
#define FDI_SAVE_TRUNCATED 1  /* Truncated data while saving */
#define FDI_SAVE_PADDED    2  /* Padded data while saving    */
#define FDI_SAVE_OK        3  /* Succeeded saving disk image */

                              /* Supported disk image formats:  */
#define FMT_AUTO           0  /* Determine format automatically */                   
#define FMT_FDI            1  /* Generic FDI image              */ 
#define FMT_ADMDSK         2  /* Coleco Adam disk 160K          */  
#define FMT_DDP            3  /* Coleco Adam tape 256K          */  
#define FMT_ADMDSK320      4  /* Coleco Adam disk 320K          */  

#define SEEK_DELETED (0x40000000)

#define DataFDI(D) ((D)->Data+(D)->Data[10]+((int)((D)->Data[11])<<8))

#ifndef BYTE_TYPE_DEFINED
#define BYTE_TYPE_DEFINED
typedef unsigned char byte;
#endif

/** FDIDisk **************************************************/
/** This structure contains all disk image information and  **/
/** also the result of the last SeekFDI() call.             **/
/*************************************************************/
typedef struct
{
  byte Format;     /* Original disk format (FMT_*) */
  byte Sides;      /* Sides per disk */
  byte Tracks;     /* Tracks per side */
  byte Sectors;    /* Sectors per track */
  int  SecSize;    /* Bytes per sector */

  byte *Data;      /* Disk data */
  int  DataSize;   /* Disk data size */

  byte Header[6];  /* Current header, result of SeekFDI() */
} FDIDisk;
    
    
#define MAX_DISKS 4
#define MAX_TAPES 4
    
extern FDIDisk Disks[MAX_DISKS];
extern FDIDisk Tapes[MAX_DISKS];
    

/** InitFDI() ************************************************/
/** Clear all data structure fields.                        **/
/*************************************************************/
void InitFDI(FDIDisk *D);

/** EjectFDI() ***********************************************/
/** Eject disk image. Free all allocated memory.            **/
/*************************************************************/
void EjectFDI(FDIDisk *D);

/** NewFDI() *************************************************/
/** Allocate memory and create new .FDI disk image of given **/
/** dimensions. Returns disk data pointer on success, 0 on  **/
/** failure.                                                **/
/*************************************************************/
byte *NewFDI(FDIDisk *D,int Sides,int Tracks,int Sectors,int SecSize);

/** FormatFDI() ***********************************************/
/** Allocate memory and create new standard disk image for a **/
/** given format. Returns disk data pointer on success, 0 on **/
/** failure.                                                 **/
/**************************************************************/
byte *FormatFDI(FDIDisk *D,int Format);

/** LoadFDI() ************************************************/
/** Load a disk image from a given file, in a given format  **/
/** (see FMT_* #defines). Guess format from the file name   **/
/** when Format=FMT_AUTO. Returns format ID on success or   **/
/** 0 on failure. When FileName=0, ejects the disk freeing  **/
/** memory and returns 0.                                   **/
/*************************************************************/
int LoadFDI(FDIDisk *D,const char *FileName,int Format);

/** SaveFDI() ************************************************/
/** Save a disk image to a given file, in a given format    **/
/** (see FMT_* #defines). Use the original format when      **/
/** when Format=FMT_AUTO. Returns FDI_SAVE_OK on success,   **/
/** FDI_SAVE_PADDED if any sectors were padded,             **/
/** FDI_SAVE_TRUNCATED if any sectors were truncated,       **/
/** FDI_SAVE_FAILED (0) if failed.                          **/
/*************************************************************/
int SaveFDI(FDIDisk *D,const char *FileName,int Format);

/** SeekFDI() ************************************************/
/** Seek to given side/track/sector. Returns sector address **/
/** on success or 0 on failure.                             **/
/*************************************************************/
byte *SeekFDI(FDIDisk *D,int Side,int Track,int SideID,int TrackID,int SectorID);

/** LinearFDI() **********************************************/
/** Seek to given sector by its linear number. Returns      **/
/** sector address on success or 0 on failure.              **/
/*************************************************************/
byte *LinearFDI(FDIDisk *D,int SectorN);

#ifdef __cplusplus
}
#endif
#endif /* FDIDISK_H */
