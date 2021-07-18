#if !defined(HEADERS_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2020 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */

#pragma pack(push, 1)
struct Bitmap_Header
{
    u16 Signature;
    u32 FileSize;
    u32 Reserved;
    u32 DataOffset;
    u32 InfoHeaderSize;
    u32 Width;
    u32 Height;
    u16 Planes;
    u16 BitsPerPixel;
    u32 Compression;
    u32 ImageSize;
    u32 XPixelsPerMeter;
    u32 YPixelsPerMeter;
    u32 ColorsUsed;
    u32 ImportantColors;
};

#define WEXP_ELEMENTS_PER_VERTEX  8   // 3 position, 3 normal, 2 texture coordinates
#define WEXP_INDEX_SIZE           2   // 2 bytes per index
#define WEXP_VERTEX_SIZE          (WEXP_ELEMENTS_PER_VERTEX*sizeof(r32))
// indices are u16
struct Wexp_Header_legacy
{
    u16 signature;       // @doc: must be 0x7877  ('wx')
    u16 vert_offset;
    u32 indices_offset;
    u32 eof_offset;
};

struct Wexp_Header
{
    u16 signature;       // @doc: must be 0x7857  ('Wx')
    u8  version;
    u16 mesh_count;
};

struct Wexp_Mesh_Header
{
    u16 signature;           // @doc: must be 0x6D57  ('Wm')
    u16 vertex_data_offset;
    u32  index_data_offset;
    u32   next_elem_offset;
};
#pragma pack(pop)

#define HEADERS_H
#endif
