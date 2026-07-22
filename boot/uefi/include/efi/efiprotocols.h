#ifndef AURELIAN_EFI_PROTOCOLS_H
#define AURELIAN_EFI_PROTOCOLS_H

#include "efi/efitypes.h"

/* File protocol */
typedef struct _EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_FILE_OPEN)(
    EFI_FILE_PROTOCOL *This,
    EFI_FILE_PROTOCOL **NewHandle,
    CHAR16 *FileName,
    UINT64 OpenMode,
    UINT64 Attributes);

typedef EFI_STATUS (EFIAPI *EFI_FILE_CLOSE)(EFI_FILE_PROTOCOL *This);
typedef EFI_STATUS (EFIAPI *EFI_FILE_DELETE)(EFI_FILE_PROTOCOL *This);
typedef EFI_STATUS (EFIAPI *EFI_FILE_READ)(
    EFI_FILE_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer);
typedef EFI_STATUS (EFIAPI *EFI_FILE_WRITE)(
    EFI_FILE_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer);
typedef EFI_STATUS (EFIAPI *EFI_FILE_SET_POSITION)(
    EFI_FILE_PROTOCOL *This,
    UINT64 Position);
typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_POSITION)(
    EFI_FILE_PROTOCOL *This,
    UINT64 *Position);
typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_INFO)(
    EFI_FILE_PROTOCOL *This,
    EFI_GUID *InformationType,
    UINTN *BufferSize,
    VOID *Buffer);

#define EFI_FILE_MODE_READ  0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE 0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE 0x8000000000000000ULL

struct _EFI_FILE_PROTOCOL {
    UINT64          Revision;
    EFI_FILE_OPEN   Open;
    EFI_FILE_CLOSE  Close;
    EFI_FILE_DELETE Delete;
    EFI_FILE_READ   Read;
    EFI_FILE_WRITE  Write;
    EFI_FILE_SET_POSITION SetPosition;
    EFI_FILE_GET_POSITION GetPosition;
    EFI_FILE_GET_INFO GetInfo;
    void *SetInfo;
    void *Flush;
};

/* Simple filesystem protocol */
typedef struct {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(
        struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
        EFI_FILE_PROTOCOL **Root);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

/* EFI_DEVICE_PATH_PROTOCOL */
typedef struct _EFI_DEVICE_PATH_PROTOCOL EFI_DEVICE_PATH_PROTOCOL;
struct _EFI_DEVICE_PATH_PROTOCOL {
    UINT8  Type;
    UINT8  SubType;
    UINT8  Length[2];
};

/* Block I/O protocol */
typedef struct {
    UINT32 Revision;
    void *Media;
    EFI_STATUS (EFIAPI *Reset)(struct _EFI_BLOCK_IO_PROTOCOL *This, BOOLEAN ExtendedVerification);
    EFI_STATUS (EFIAPI *ReadBlocks)(struct _EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId,
                                     EFI_LBA Lba, UINTN BufferSize, VOID *Buffer);
    EFI_STATUS (EFIAPI *WriteBlocks)(struct _EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId,
                                      EFI_LBA Lba, UINTN BufferSize, VOID *Buffer);
    EFI_STATUS (EFIAPI *FlushBlocks)(struct _EFI_BLOCK_IO_PROTOCOL *This);
} EFI_BLOCK_IO_PROTOCOL;

typedef struct _EFI_BLOCK_IO_PROTOCOL EFI_BLOCK_IO_PROTOCOL;
typedef UINT64 EFI_LBA;
typedef struct {
    UINT32  MediaId;
    BOOLEAN RemovableMedia;
    BOOLEAN MediaPresent;
    BOOLEAN LogicalPartition;
    BOOLEAN ReadOnly;
    BOOLEAN WriteCachingEnabled;
    UINT32  BlockSize;
    UINT32  IoAlign;
    EFI_LBA LastBlock;
    BOOLEAN WriteOnce;
} EFI_BLOCK_IO_MEDIA;

/* Disk I/O protocol */
typedef struct _EFI_DISK_IO_PROTOCOL EFI_DISK_IO_PROTOCOL;
struct _EFI_DISK_IO_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *ReadDisk)(EFI_DISK_IO_PROTOCOL *This, UINT32 MediaId,
                                   UINT64 Offset, UINTN BufferSize, VOID *Buffer);
    EFI_STATUS (EFIAPI *WriteDisk)(EFI_DISK_IO_PROTOCOL *This, UINT32 MediaId,
                                    UINT64 Offset, UINTN BufferSize, VOID *Buffer);
};

/* Loaded Image protocol */
typedef struct {
    UINT32          Revision;
    EFI_HANDLE      ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE      DeviceHandle;
    EFI_DEVICE_PATH_PROTOCOL *FilePath;
    VOID            *Reserved;
    UINT32          LoadOptionsSize;
    VOID            *LoadOptions;
    EFI_IMAGE_UNLOAD  Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

typedef void (EFIAPI *EFI_IMAGE_UNLOAD)(EFI_HANDLE ImageHandle);

/* Graphics Output Protocol */
typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    UINT32  MaxMode;
    UINT32  Mode;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINT32  FrameBufferSize;
    UINT32  HorizontalResolution;
    UINT32  VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    UINT32  PixelInformation;
    UINT32  PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax,
};

typedef struct {
    UINT32  MaxMode;
    UINT32  Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN  SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN  FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *QueryMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 ModeNumber,
                                    UINTN *SizeOfInfo,
                                    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info);
    EFI_STATUS (EFIAPI *SetMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 ModeNumber);
    EFI_STATUS (EFIAPI *Blt)(EFI_GRAPHICS_OUTPUT_PROTOCOL *This, void *BltBuffer,
                              UINT32 BltOperation, UINTN SourceX, UINTN SourceY,
                              UINTN DestinationX, UINTN DestinationY,
                              UINTN Width, UINTN Height, UINTN Delta);
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};

/* Acpi table protocols */
#define EFI_ACPI_TABLE_GUID \
    { 0xdeb6302a, 0x8c0f, 0x4c25, { 0x8d, 0x70, 0x3b, 0xb0, 0x29, 0x43, 0x16, 0x56 } }

#endif
