#ifndef AURELIAN_EFI_TYPES_H
#define AURELIAN_EFI_TYPES_H

#include <stdint.h>

typedef uint8_t  UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef int8_t   INT8;
typedef int16_t  INT16;
typedef int32_t  INT32;
typedef int64_t  INT64;

typedef UINT8   BOOLEAN;
typedef UINT8   UINTN;
typedef INTN    INTN;
typedef char    CHAR8;
typedef uint16_t CHAR16;

typedef void *EFI_HANDLE;
typedef UINTN EFI_STATUS;

#define EFI_SUCCESS             0
#define EFI_LOAD_ERROR          1
#define EFI_INVALID_PARAMETER   2
#define EFI_UNSUPPORTED         3
#define EFI_BAD_BUFFER_SIZE     4
#define EFI_BUFFER_TOO_SMALL    5
#define EFI_NOT_READY           6
#define EFI_DEVICE_ERROR        7
#define EFI_WRITE_PROTECTED     8
#define EFI_OUT_OF_RESOURCES    9
#define EFI_VOLUME_CORRUPTED    10
#define EFI_VOLUME_FULL         11
#define EFI_NO_MEDIA            12
#define EFI_MEDIA_CHANGED       13
#define EFI_NOT_FOUND           14
#define EFI_ACCESS_DENIED        15
#define EFI_NO_RESPONSE         16
#define EFI_NO_MAPPING          17
#define EFI_TIMEOUT             18
#define EFI_NOT_STARTED         19
#define EFI_ALREADY_STARTED     20
#define EFI_ABORTED             21
#define EFI_PROTOCOL_ERROR      23
#define EFI_NOT_FOUND           14

/* Memory types */
#define EFI_RESERVED_MEMORY_TYPE       0
#define EFI_LOADER_CODE                 1
#define EFI_LOADER_DATA                 2
#define EFI_BOOT_SERVICES_CODE          3
#define EFI_BOOT_SERVICES_DATA          4
#define EFI_RUNTIME_SERVICES_CODE      5
#define EFI_RUNTIME_SERVICES_DATA      6
#define EFI_CONVENTIONAL_MEMORY         7
#define EFI_UNUSABLE_MEMORY            8
#define EFI_ACPI_RECLAIM_MEMORY        9
#define EFI_ACPI_MEMORY_NVS            10
#define EFI_MEMORY_MAPPED_IO           11
#define EFI_MEMORY_MAPPED_IO_PORT_SPACE 12
#define EFI_PAL_CODE                  13
#define EFI_PERSISTENT_MEMORY         14

/* Memory allocation types */
#define EFI_ALLOCATE_ANY_PAGES     0
#define EFI_ALLOCATE_MAX_ADDRESS   1
#define EFI_ALLOCATE_ADDRESS       2

/* Memory map attributes */
#define EFI_MEMORY_UC   0x0000000000000001ULL
#define EFI_MEMORY_WC   0x0000000000000002ULL
#define EFI_MEMORY_WT   0x0000000000000004ULL
#define EFI_MEMORY_WB   0x0000000000000008ULL
#define EFI_MEMORY_UCE  0x0000000000000010ULL
#define EFI_MEMORY_WP   0x0000000000001000ULL
#define EFI_MEMORY_RP   0x0000000000002000ULL
#define EFI_MEMORY_XP   0x0000000000004000ULL
#define EFI_MEMORY_NV   0x0000000000008000ULL
#define EFI_MEMORY_RO   0x0000000000020000ULL
#define EFI_MEMORY_RUNTIME  0x8000000000000000ULL

/* Locate search type */
#define EFI_LOCATE_ALL_HANDLES       0
#define EFI_LOCATE_BY_REGISTER_NOTIFY 1
#define EFI_LOCATE_BY_PROTOCOL       2

/* Physical address */
typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef uint64_t EFI_VIRTUAL_ADDRESS;

/* GUID structure */
typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

/* Time structure */
typedef struct {
    UINT16  Year;
    UINT8   Month;
    UINT8   Day;
    UINT8   Hour;
    UINT8   Minute;
    UINT8   Second;
    UINT8   Pad1;
    UINT32  Nanosecond;
    INT16   TimeZone;
    UINT8   Daylight;
    UINT8   Pad2;
} EFI_TIME;

/* Capsule block descriptor */
typedef struct {
    UINT64 Length;
    union {
        EFI_PHYSICAL_ADDRESS DataBlock;
        EFI_PHYSICAL_ADDRESS ContinuationPointer;
    } Union;
} EFI_CAPSULE_BLOCK_DESCRIPTOR;

/* Simple text output protocol */
typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16 *String);

typedef struct {
    INT32 MaxMode;
    INT32 Mode;
    INT32 Attribute;
    INT32 CursorColumn;
    INT32 CursorRow;
    BOOLEAN CursorVisible;
} EFI_SIMPLE_TEXT_OUTPUT_MODE;

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_TEXT_STRING        Reset;
    EFI_TEXT_STRING        OutputString;
    EFI_TEXT_STRING        TestString;
    EFI_TEXT_STRING        QueryMode;
    EFI_TEXT_STRING        SetMode;
    EFI_TEXT_STRING        SetAttribute;
    EFI_TEXT_STRING        ClearScreen;
    EFI_TEXT_STRING        SetCursorPosition;
    EFI_TEXT_STRING        EnableCursor;
    EFI_SIMPLE_TEXT_OUTPUT_MODE *Mode;
};

/* Simple text input protocol */
typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    void *Reset;
    EFI_STATUS (*ReadKeyStroke)(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, void *Key);
    void *WaitForKey;
};

#endif /* AURELIAN_EFI_TYPES_H */
