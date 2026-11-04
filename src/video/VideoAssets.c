#include "nwarch.h"
#include "nwdebug.h"
/*
 * VideoAssets -- sprite/tile loading and screen geometry.
 *
 * Module: video
 * Interface: include/VideoAssets.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   FlushPendingSurfaceRectsAndReseedRandom                    video/FlushPendingSurfaceRectsAndReseedRandom.c
 *   LoadDiskSpriteImageFromStream                              video/LoadDiskSpriteImageFromStream.c
 *   LoadNetwalkTileImageFile                                   video/LoadNetwalkTileImageFile.c
 *   PublishScreenGeometryAndClaimFrameBuffer                   video/PublishScreenGeometryAndClaimFrameBuffer.c
 *   TryPlayStartupMovieWithWindowFallback                      video/TryPlayStartupMovieWithWindowFallback.c
 */

/* ==========================================================================
 * Unit: FlushPendingSurfaceRectsAndReseedRandom
 * Source: video/FlushPendingSurfaceRectsAndReseedRandom.c
 * ========================================================================== */
#ifdef NW_UNIT_FlushPendingSurfaceRectsAndReseedRandom
/*
 * Flush Pending Surface Rects And Reseed Random.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-flushpendingsurfacerectsandreseedrandom-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

#define NW8(off)  (*(uint8_t  *)(intptr_t)(nw_data_window + (off)))
#define NW32(off) (*(uint32_t *)(intptr_t)(nw_data_window + (off)))

extern uint32_t dwRngState;

extern void __stdcall BlitRectOrTileGridToPlayerSurface(
    char cTileGridMode, int nRectBottom, int nRectRight, int nRectTop,
    int nRectLeft);
extern void __stdcall FreeHeapBlock(uint8_t *pData);
extern void FlushGdiDrawingBatch(void);                 /* cdecl */
extern int32_t CaptureLocalTimeAsEpochSeconds(int32_t *outSeconds); /* cdecl */
extern void __stdcall SelectNextActiveSlot(uint8_t *pOldSlot);

NW_DIAG void __stdcall
FlushPendingSurfaceRectsAndReseedRandom(char bDrawPending)
    __asm__("_FlushPendingSurfaceRectsAndReseedRandom");

NW_DIAG void __stdcall
FlushPendingSurfaceRectsAndReseedRandom(char bDrawPending)
{
    uint32_t *pNode = (uint32_t *)(uintptr_t)NW_WINDOW_PTR_VALUE(0x11b0);

    while (pNode != NULL) {
        uint32_t *pNext = (uint32_t *)(uintptr_t)
                          (*(uint32_t *)(void *)((uint8_t *)(void *)pNode + 0x10));
        if (bDrawPending != 0) {

            BlitRectOrTileGridToPlayerSurface(
                (char)0, (int)pNode[3], (int)pNode[2], (int)pNode[1],
                (int)pNode[0]);
        }
        FreeHeapBlock((uint8_t *)(void *)pNode);
        pNode = pNext;
    }

    FlushGdiDrawingBatch();
    NW_WINDOW_PTR_VALUE(0x11b0) = 0u;
    NW8(0x67fd)  = 0xffu;
    NW8(0x67fe)  = 0x0eu;
    NW32(0x6388) = 0u;

    dwRngState = (uint32_t)CaptureLocalTimeAsEpochSeconds(NULL);
    dwRngState = dwRngState * 0x41c64e6du + 0x3039u;

    NW_WINDOW_PTR_VALUE(0x11b4) = 0u;
    SelectNextActiveSlot(NULL);
}

#if !defined(__ELF__)
__asm__(".globl FlushPendingSurfaceRectsAndReseedRandom\n"
        ".set FlushPendingSurfaceRectsAndReseedRandom, "
        "_FlushPendingSurfaceRectsAndReseedRandom\n");
__asm__(".globl _FlushPendingSurfaceRectsAndReseedRandom@4\n"
        ".set _FlushPendingSurfaceRectsAndReseedRandom@4, "
        "_FlushPendingSurfaceRectsAndReseedRandom\n");
#endif
#endif /* NW_UNIT_FlushPendingSurfaceRectsAndReseedRandom */

/* ==========================================================================
 * Unit: LoadDiskSpriteImageFromStream
 * Source: video/LoadDiskSpriteImageFromStream.c
 * ========================================================================== */
#ifdef NW_UNIT_LoadDiskSpriteImageFromStream
/*
 * Load Disk Sprite Image From Stream.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-loaddiskspriteimagefromstream-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];

#define DISK_IMAGE_TAG 0x4469736bUL

/* @(nw_data_window + 0x16e8) - offset whose image is currently resident in g_pbSpritePixelBuf.
 * Load-cache key: equal means "already decoded, do no stream traffic". */
#define GLOBAL_RESIDENT_OFFSET   (*(DWORD  *)(nw_data_window + 0x16e8))

/* @(nw_data_window + 0x16f0) - shared sprite pixel buffer; destination of the raw copy and of
 * the LZSS expansion, and the base of the returned pointer. */
#define GLOBAL_SPRITE_PIXEL_BUF  NW_WINDOW_PTR_REF(BYTE, 0x16f0)

/* The stream offset at record +0x08 is a LOGICAL offset from the start of the
 * save file, which is the origin SeekStreamLogicalOffset is given here. */
#define SEEK_BEGIN_STREAM 0

#define LZSS_IMAGE_HEADER_BYTES 4

extern BOOL __stdcall SeekStreamLogicalOffset(DWORD dwMoveMethod,
                                              int    nLogicalOffset);

extern int __stdcall ReadExactChunkFromStream(DWORD cbRequestedBytes,
                                            char  *lpBuffer);

extern void __cdecl DecompressLzssBlockToBuffer(int  cbSrc,
                                                 BYTE *pbDst,
                                                 BYTE *pbSrc);

extern void *__stdcall GetHeapArenaBlockEnd(DWORD dwRequestSize);

#ifdef NW_PLATFORM_SDL
/* ==========================================================================
 * External asset pack (SDL build only)
 *
 * The shipped overlay is a serialized VM stream, not a flat archive: the
 * image records are synthesized by opcodes at run time and only then carry a
 * "Disk" tag plus a logical stream offset.  Rather than re-implement the VM in
 * a separate unpacker, the engine can dump every decoded sprite as it loads:
 *
 *   NW_DEBUG_ASSET_DUMP=/tmp/nwassets  ->  sprites/<offset>_<w>x<h>.idx
 *                                  sprites.tsv (offset, w, h, stored, crc32)
 *
 * A pack directory with the same sprites.tsv is an override pack:
 *
 *   NW_DEBUG_ASSET_PACK=/tmp/nwpack
 *
 * A manifest row is tab/space separated:
 *
 *   offset  width  height  stored  crc32  file  [source_width source_height]
 *
 * The optional source dimensions let a high-resolution replacement be larger
 * than the original.  It is downsampled with nearest-neighbour sampling to the
 * geometry the renderer already expects, so no game layout changes.
 * ========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <SDL3/SDL_timer.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#endif

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
extern void platform_log_mark(const char *stage);
#define NW_ASSET_LOG(stage) platform_log_mark(stage)
#else
#define NW_ASSET_LOG(stage) ((void)0)
#endif

#define NW_ASSET_PATH_MAX 1024
#define NW_ASSET_MAX_ENTRIES 8192
#define NW_ASSET_MAX_NAME 256

#if defined(__ELF__)
#  define NW_ASSET_SEP "/"
#  define NW_ASSET_DUMP_REL "sprites/"
#else
#  define NW_ASSET_SEP "\\"
#  define NW_ASSET_DUMP_REL "sprites\\"
#endif

typedef struct NwAssetEntry {
    DWORD    dwOffset;
    DWORD    dwCrc32;
    WORD     w;
    WORD     h;
    WORD     sw;
    WORD     sh;
    DWORD    dwDataOffset;   /* embedded overlay pack; 0 for a file entry */
    DWORD    dwDataLen;
    char     szFile[NW_ASSET_MAX_NAME];
} NwAssetEntry;

static int          g_assetDumpReady;
static char         g_assetDumpRoot[NW_ASSET_PATH_MAX];
static FILE        *g_assetManifest;
static DWORD       *g_assetDumpOffsets;
static int          g_assetDumpCount;

static int          g_assetOverrideReady;
static NwAssetEntry *g_assetOverrides;
static int          g_assetOverrideCount;
static char         g_assetOverrideRoot[NW_ASSET_PATH_MAX];
static char         g_assetOverlayPath[NW_ASSET_PATH_MAX];
static FILE        *g_assetLog;

/* Runtime asset-source selection.  The high-resolution pack and the original
 * VM images both remain available for the whole session; this flag only
 * chooses which source the loader and the scaled blitter consult.  It starts
 * enabled so existing 4x runs are unchanged. */
static int          g_assetHighResEnabled = 1;

/* Embedded high-resolution section, immediately before the original 38-byte
 * overlay trailer.  All offsets in the table are relative to the section
 * start, so the same bytes work in overlay.nw and in the materialized
 * overlay.bin + overlay.trailer form (which adds the 0x12800 dbase pad). */
#define NW_HR_MAGIC       "NWHRPL01"
#define NW_HR_VERSION     1u
#define NW_HR_FOOTER_SIZE 32u
#define NW_HR_ENTRY_SIZE  24u

/* Synthetic table entry that carries the upscaled full-screen background.
 * Real sprite offsets are stream positions (well below 5 MB), so this value
 * cannot collide with a descriptor. */
#define NW_ASSET_BACKGROUND_OFFSET 0x4E574247u

void NwAssetSetOverlayPath(const char *pszPath)
{
    const char *src = pszPath;
    char       *dst = g_assetOverlayPath;
    size_t      left = sizeof g_assetOverlayPath - 1;

    if (src == NULL)
        src = "";
#if defined(__ELF__)
    /* The Linux compat layer reports Win32-style module paths such as
     * "Z:\\home\\user\\game\\overlay.nw".  fopen() needs the POSIX form. */
    if (strlen(src) >= 3 &&
        ((src[0] >= 'A' && src[0] <= 'Z') ||
         (src[0] >= 'a' && src[0] <= 'z')) &&
        src[1] == ':' && (src[2] == '\\' || src[2] == '/')) {
        src += 2;
    }
    while (*src != '\0' && left > 0) {
        *dst++ = (*src == '\\') ? '/' : *src;
        src++;
        left--;
    }
#else
    while (*src != '\0' && left > 0) {
        *dst++ = *src++;
        left--;
    }
#endif
    *dst = '\0';
}

/* Asset-source switch used by the SDL input loop.  The high-resolution pack
 * stays loaded while the original source is selected, so switching back never
 * touches the 54 MB pool or the overlay file.  The logical sprite buffer is
 * shared by both modes; invalidate its one-entry load cache so the next frame
 * refills it from the stream under the newly selected mode. */
int NwAssetHighResEnabled(void)
{
    return g_assetHighResEnabled;
}

void NwAssetSetHighResEnabled(int bEnabled)
{
    extern int NwSdlRenderScale(void);
    extern int NwBlitRedrawFullFrame(void);

    bEnabled = (bEnabled != 0);
    if (bEnabled == g_assetHighResEnabled)
        return;
    g_assetHighResEnabled = bEnabled;
    if (NwSdlRenderScale() > 1) {
        GLOBAL_RESIDENT_OFFSET = 0xffffffffu;
        NwBlitRedrawFullFrame();
    }

    {
        const char *pszMode = g_assetHighResEnabled
                            ? "asset_mode_highres"
                            : "asset_mode_original";
        NW_ASSET_LOG(pszMode);
        if (g_assetLog != NULL) {
            fprintf(g_assetLog, "%s\n", pszMode);
            fflush(g_assetLog);
        }
    }
}

void NwAssetToggleHighRes(void)
{
    NwAssetSetHighResEnabled(!g_assetHighResEnabled);
}

static DWORD NwAssetCrc32(const BYTE *pb, DWORD cb)
{
    DWORD crc = 0xffffffffu;
    DWORD i;
    int   bit;

    for (i = 0; i < cb; i++) {
        crc ^= pb[i];
        for (bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xedb88320u & (DWORD)(-(int)(crc & 1u)));
    }
    return crc ^ 0xffffffffu;
}

static void NwAssetMakeDir(const char *psz)
{
    (void)CreateDirectoryA(psz, NULL);
}

static void NwAssetDumpInit(void)
{
    const char *pszRoot;
    char        szPath[NW_ASSET_PATH_MAX];

    if (g_assetDumpReady)
        return;
    g_assetDumpReady = 1;

    pszRoot = NW_DEBUG_GETENV("NW_DEBUG_ASSET_DUMP");
    if (pszRoot == NULL || pszRoot[0] == '\0')
        return;
    if (strlen(pszRoot) >= sizeof g_assetDumpRoot)
        return;
    strcpy(g_assetDumpRoot, pszRoot);

    NwAssetMakeDir(g_assetDumpRoot);
    snprintf(szPath, sizeof szPath, "%s" NW_ASSET_SEP "sprites", g_assetDumpRoot);
    NwAssetMakeDir(szPath);

    g_assetDumpOffsets = (DWORD *)malloc(sizeof(DWORD) * NW_ASSET_MAX_ENTRIES);
    if (g_assetDumpOffsets == NULL)
        return;

    snprintf(szPath, sizeof szPath, "%s" NW_ASSET_SEP "sprites.tsv", g_assetDumpRoot);
    g_assetManifest = fopen(szPath, "w");
    if (g_assetManifest != NULL)
        fprintf(g_assetManifest,
                "# nwassets 1\n"
                "# offset\twidth\theight\tstored\tcrc32\tfile\t[src_width\tsrc_height]\n");
}

static void NwAssetDumpSprite(DWORD dwOffset, WORD w, WORD h, DWORD dwStored,
                              const BYTE *pbPixels)
{
    char  szPath[NW_ASSET_PATH_MAX];
    char  szRel[NW_ASSET_MAX_NAME];
    FILE *f;
    DWORD crc;
    int   i;

    NwAssetDumpInit();
    if (g_assetManifest == NULL || pbPixels == NULL || w == 0 || h == 0)
        return;

    /* A few sprites are shared by many records; the offset is the cache key,
     * so one file per offset is enough. */
    for (i = 0; i < g_assetDumpCount; i++) {
        if (g_assetDumpOffsets[i] == dwOffset)
            return;
    }
    if (g_assetDumpCount < NW_ASSET_MAX_ENTRIES)
        g_assetDumpOffsets[g_assetDumpCount++] = dwOffset;

    crc = NwAssetCrc32(pbPixels, (DWORD)w * (DWORD)h);
    snprintf(szRel, sizeof szRel, NW_ASSET_DUMP_REL "o%08x_%ux%u.idx",
             (unsigned)dwOffset, (unsigned)w, (unsigned)h);
    snprintf(szPath, sizeof szPath, "%s" NW_ASSET_SEP "%s", g_assetDumpRoot, szRel);
    f = fopen(szPath, "wb");
    if (f == NULL)
        return;
    if (fwrite(pbPixels, 1, (size_t)w * (size_t)h, f) != (size_t)w * (size_t)h) {
        fclose(f);
        return;
    }
    fclose(f);

    fprintf(g_assetManifest, "%08x\t%u\t%u\t%08x\t%08x\t%s\n",
            (unsigned)dwOffset, (unsigned)w, (unsigned)h,
            (unsigned)dwStored, (unsigned)crc, szRel);
    fflush(g_assetManifest);
}

static DWORD NwAssetReadLe32(const BYTE *pb)
{
    return (DWORD)pb[0] | ((DWORD)pb[1] << 8) |
           ((DWORD)pb[2] << 16) | ((DWORD)pb[3] << 24);
}

static WORD NwAssetReadLe16(const BYTE *pb)
{
    return (WORD)((WORD)pb[0] | ((WORD)pb[1] << 8));
}

static int NwAssetLoadEmbeddedPack(void)
{
    BYTE  footer[NW_HR_FOOTER_SIZE];
    BYTE  head[4];
    BYTE  row[NW_HR_ENTRY_SIZE];
    FILE *f;
    long  cbFile;
    long  footerPos;
    DWORD sectionBytes;
    DWORD tableBytes;
    DWORD count;
    DWORD version;
    DWORD sectionStart;
    DWORD i;

    if (g_assetOverlayPath[0] == '\0' || g_assetOverrides == NULL)
        return 0;
    f = fopen(g_assetOverlayPath, "rb");
    if (f == NULL)
        return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    cbFile = ftell(f);
    if (cbFile < (long)(38 + NW_HR_FOOTER_SIZE)) { fclose(f); return 0; }
    footerPos = cbFile - 38 - (long)NW_HR_FOOTER_SIZE;
    if (fseek(f, footerPos, SEEK_SET) != 0) { fclose(f); return 0; }
    if (fread(footer, 1, sizeof footer, f) != sizeof footer) {
        fclose(f);
        return 0;
    }
    if (memcmp(footer, NW_HR_MAGIC, 8) != 0) { fclose(f); return 0; }
    version      = NwAssetReadLe32(footer + 8);
    sectionBytes = NwAssetReadLe32(footer + 12);
    tableBytes   = NwAssetReadLe32(footer + 16);
    count        = NwAssetReadLe32(footer + 20);
    if (version != NW_HR_VERSION || count == 0 ||
        count > NW_ASSET_MAX_ENTRIES ||
        sectionBytes == 0 || tableBytes < 4 ||
        tableBytes > sectionBytes ||
        sectionBytes > (DWORD)footerPos) {
        fclose(f);
        return 0;
    }
    sectionStart = (DWORD)(footerPos - (long)sectionBytes);
    if (fseek(f, (long)sectionStart, SEEK_SET) != 0) { fclose(f); return 0; }
    if (fread(head, 1, sizeof head, f) != sizeof head ||
        NwAssetReadLe32(head) != count) {
        fclose(f);
        return 0;
    }
    {
        DWORD tableFixed = 4u + count * NW_HR_ENTRY_SIZE;
        if (tableBytes < tableFixed ||
            fseek(f, (long)(tableBytes - tableFixed), SEEK_CUR) != 0) {
            fclose(f);
            return 0;
        }
    }
    for (i = 0; i < count; i++) {
        DWORD off, crc, dataOff, dataLen;
        WORD  w, h, sw, sh;
        NwAssetEntry *e;

        if (fread(row, 1, sizeof row, f) != sizeof row) {
            g_assetOverrideCount = 0;
            fclose(f);
            return 0;
        }
        off     = NwAssetReadLe32(row + 0);
        crc     = NwAssetReadLe32(row + 4);
        w       = NwAssetReadLe16(row + 8);
        h       = NwAssetReadLe16(row + 10);
        sw      = NwAssetReadLe16(row + 12);
        sh      = NwAssetReadLe16(row + 14);
        dataOff = NwAssetReadLe32(row + 16);
        dataLen = NwAssetReadLe32(row + 20);
        if (w == 0 || h == 0 || sw == 0 || sh == 0 ||
            sw < w || sh < h ||
            dataLen != (DWORD)sw * (DWORD)sh ||
            dataOff < tableBytes ||
            dataOff > sectionBytes ||
            dataLen > sectionBytes - dataOff ||
            off < 0x10000u) {
            g_assetOverrideCount = 0;
            fclose(f);
            return 0;
        }
        e = &g_assetOverrides[g_assetOverrideCount++];
        e->dwOffset     = off;
        e->dwCrc32      = crc;
        e->w            = w;
        e->h            = h;
        e->sw           = sw;
        e->sh           = sh;
        e->dwDataOffset = sectionStart + dataOff;
        e->dwDataLen    = dataLen;
        e->szFile[0]    = '\0';
    }
    fclose(f);
    return 1;
}

static void NwAssetOverrideInit(void)
{
    const char *pszRoot;
    const char *pszLog;
    char        szPath[NW_ASSET_PATH_MAX];
    char        szLine[NW_ASSET_PATH_MAX + NW_ASSET_MAX_NAME];
    FILE       *f;

    if (g_assetOverrideReady)
        return;
    g_assetOverrideReady = 1;

    g_assetOverrides = (NwAssetEntry *)malloc(sizeof(NwAssetEntry)
                                               * NW_ASSET_MAX_ENTRIES);
    if (g_assetOverrides == NULL)
        return;

    pszRoot = NW_DEBUG_GETENV("NW_DEBUG_ASSET_PACK");
    if (pszRoot != NULL && pszRoot[0] != '\0' &&
        strlen(pszRoot) < sizeof g_assetOverrideRoot) {
        strcpy(g_assetOverrideRoot, pszRoot);
        snprintf(szPath, sizeof szPath, "%s" NW_ASSET_SEP "sprites.tsv",
                 g_assetOverrideRoot);
        f = fopen(szPath, "r");
        if (f == NULL) {
            snprintf(szPath, sizeof szPath, "%s" NW_ASSET_SEP "manifest.tsv",
                     g_assetOverrideRoot);
            f = fopen(szPath, "r");
        }
        if (f != NULL) {
            while (fgets(szLine, sizeof szLine, f) != NULL) {
                NwAssetEntry *e;
                unsigned     off, crc, stored, w, h, sw, sh;
                char         szName[NW_ASSET_MAX_NAME];
                int          n;

                if (szLine[0] == '#' || szLine[0] == '\0' || szLine[0] == '\n')
                    continue;
                szName[0] = '\0';
                n = sscanf(szLine, "%x %u %u %x %x %255s %u %u",
                           &off, &w, &h, &stored, &crc, szName, &sw, &sh);
                (void)stored;   /* recorded for provenance; not needed */
                if (n < 6 || szName[0] == '\0' || w == 0 || h == 0)
                    continue;
                if (n < 8) {
                    sw = w;
                    sh = h;
                }
                if (sw == 0 || sh == 0)
                    continue;
                if (g_assetOverrideCount >= NW_ASSET_MAX_ENTRIES)
                    break;
                e = &g_assetOverrides[g_assetOverrideCount++];
                e->dwOffset     = (DWORD)off;
                e->dwCrc32      = (DWORD)crc;
                e->w            = (WORD)w;
                e->h            = (WORD)h;
                e->sw           = (WORD)sw;
                e->sh           = (WORD)sh;
                e->dwDataOffset = 0;
                e->dwDataLen    = 0;
                strncpy(e->szFile, szName, sizeof e->szFile - 1);
                e->szFile[sizeof e->szFile - 1] = '\0';
#if defined(__ELF__)
                {
                    char *q;
                    for (q = e->szFile; *q != '\0'; q++) {
                        if (*q == '\\')
                            *q = '/';
                    }
                }
#endif
            }
            fclose(f);
        }
    }

    /* A high-resolution overlay carries its own pack.  An explicit NW_DEBUG_ASSET_PACK
     * pack keeps priority; if that module had no usable manifest we fall back
     * to the section embedded in overlay.nw/overlay.bin. */
    if (g_assetOverrideCount == 0)
        (void)NwAssetLoadEmbeddedPack();

    pszLog = NW_DEBUG_GETENV("NW_DEBUG_ASSET_LOG");
    if (pszLog != NULL && pszLog[0] != '\0')
        g_assetLog = fopen(pszLog, "w");
}

static const NwAssetEntry *NwAssetFindOverride(DWORD dwOffset, DWORD dwCrc32,
                                               WORD w, WORD h)
{
    int i;

    for (i = 0; i < g_assetOverrideCount; i++) {
        const NwAssetEntry *e = &g_assetOverrides[i];
        if (e->dwOffset == NW_ASSET_BACKGROUND_OFFSET)
            continue;               /* background pixels are never sprites */
        if (e->w != w || e->h != h)
            continue;
        if (e->dwOffset == dwOffset || e->dwCrc32 == dwCrc32)
            return e;
    }
    return NULL;
}

static void NwAssetApplyOverride(DWORD dwOffset, WORD w, WORD h,
                                 BYTE *pbPixels)
{
    const NwAssetEntry *e;
    char  szPath[NW_ASSET_PATH_MAX];
    FILE *f;
    BYTE *pbSrc;
    long  cbFile;
    DWORD crc;
    unsigned x, y;

    if (!g_assetHighResEnabled)
        return;
    NwAssetOverrideInit();
    if (g_assetOverrideCount == 0 || pbPixels == NULL || w == 0 || h == 0)
        return;

    crc = NwAssetCrc32(pbPixels, (DWORD)w * (DWORD)h);
    e = NwAssetFindOverride(dwOffset, crc, w, h);
    if (e == NULL)
        return;

    if (e->szFile[0] == '\0') {
        /* The embedded pack is a render-scale feature.  At scale 1 leave the
         * original VM image untouched, so the same overlay behaves exactly
         * like the reference build. */
        extern int NwSdlRenderScale(void);
        if (NwSdlRenderScale() <= 1)
            return;
        if (g_assetOverlayPath[0] == '\0')
            return;
        f = fopen(g_assetOverlayPath, "rb");
        if (f == NULL)
            return;
        if (fseek(f, (long)e->dwDataOffset, SEEK_SET) != 0) {
            fclose(f);
            return;
        }
        cbFile = (long)e->dwDataLen;
    } else {
        snprintf(szPath, sizeof szPath, "%s" NW_ASSET_SEP "%s",
                 g_assetOverrideRoot, e->szFile);
        f = fopen(szPath, "rb");
        if (f == NULL)
            return;
        if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return; }
        cbFile = ftell(f);
        if (cbFile <= 0) { fclose(f); return; }
        rewind(f);
    }

    pbSrc = (BYTE *)malloc((size_t)cbFile);
    if (pbSrc == NULL) { fclose(f); return; }
    if (fread(pbSrc, 1, (size_t)cbFile, f) != (size_t)cbFile) {
        free(pbSrc); fclose(f); return;
    }
    fclose(f);

    /* Trust the manifest dimensions only when they agree with the file. */
    if ((long)e->sw * (long)e->sh != cbFile) {
        if ((long)w * (long)h == cbFile) {
            e = NULL;  /* same-size fallback, handled below */
        } else {
            free(pbSrc);
            return;
        }
    }

    if (e != NULL && (e->sw != w || e->sh != h)) {
        /* Nearest-neighbour reduction/expansion.  Palette indices must not be
         * averaged: the result has to stay a valid index into the game palette. */
        for (y = 0; y < h; y++) {
            unsigned sy = (unsigned)(((unsigned long)y * e->sh) / h);
            for (x = 0; x < w; x++) {
                unsigned sx = (unsigned)(((unsigned long)x * e->sw) / w);
                pbPixels[(size_t)y * w + x] = pbSrc[(size_t)sy * e->sw + sx];
            }
        }
    } else {
        memcpy(pbPixels, pbSrc, (size_t)w * h);
    }

    if (g_assetLog != NULL) {
        fprintf(g_assetLog, "sprite off=%08x orig=%ux%u crc=%08x file=%s src=%ux%u\n",
                (unsigned)dwOffset, (unsigned)w, (unsigned)h, (unsigned)crc,
                e != NULL ? e->szFile : "<same-size>",
                e != NULL ? (unsigned)e->sw : (unsigned)w,
                e != NULL ? (unsigned)e->sh : (unsigned)h);
        fflush(g_assetLog);
    }

    free(pbSrc);
}

/* High-resolution sprite lookup for the native render-scale blitter.  The
 * normal override path above still produces the logical-size copy that the
 * original 640x480 path consumes.  This side cache keeps the full source
 * pixels available when the framebuffer itself is larger. */
typedef struct NwAssetHighRes {
    DWORD          dwOffset;
    DWORD          dwCrc32;
    BYTE          *pbPixels;
    WORD           w;
    WORD           h;
    WORD           sw;
    WORD           sh;
} NwAssetHighRes;

static NwAssetHighRes *g_assetHighRes;
static int             g_assetHighResCount;
static int             g_assetHighResCapacity;

static NwAssetHighRes *nw_asset_high_res_append(void)
{
    if (g_assetHighResCount >= g_assetHighResCapacity) {
        int newCapacity = g_assetHighResCapacity ? g_assetHighResCapacity * 2
                                                  : 64;
        NwAssetHighRes *cache = (NwAssetHighRes *)realloc(
            g_assetHighRes, (size_t)newCapacity * sizeof(*g_assetHighRes));
        if (cache == NULL)
            return NULL;
        g_assetHighRes = cache;
        g_assetHighResCapacity = newCapacity;
    }
    return &g_assetHighRes[g_assetHighResCount++];
}

static int nw_asset_high_res_has(DWORD dwOffset, DWORD dwCrc32, WORD w, WORD h)
{
    int i;

    for (i = 0; i < g_assetHighResCount; i++) {
        const NwAssetHighRes *entry = &g_assetHighRes[i];
        if (entry->w != w || entry->h != h)
            continue;
        if (entry->dwOffset == dwOffset)
            return 1;
        if (dwCrc32 != 0 && entry->dwCrc32 == dwCrc32)
            return 1;
    }
    return 0;
}

/* Per-frame accounting for the main loop's stall log.  A high-resolution
 * sprite is loaded from the overlay on first use; a first-level or first-menu
 * load can otherwise look like an unexplained game-logic hitch. */
static uint64_t g_assetLoadNs;
static unsigned g_assetLoadCalls;
static unsigned g_assetLoadMisses;

void NwAssetTakeLoadStats(uint64_t *pNs, unsigned *pCalls, unsigned *pMisses)
{
    if (pNs != NULL)
        *pNs = g_assetLoadNs;
    if (pCalls != NULL)
        *pCalls = g_assetLoadCalls;
    if (pMisses != NULL)
        *pMisses = g_assetLoadMisses;
    g_assetLoadNs = 0;
    g_assetLoadCalls = 0;
    g_assetLoadMisses = 0;
}

/* Inline images have no stream offset: their record begins with the geometry
 * words and the pixel bytes follow at +4.  The blitter uses this to choose CRC
 * matching instead of treating the first pixel dword as an offset. */
int NwAssetImageRecordIsDisk(const void *pRecord)
{
    const BYTE *pb = (const BYTE *)pRecord;
    if (pb == NULL)
        return 0;
    return *(const DWORD *)(pb + 4) == DISK_IMAGE_TAG;
}

/* Look up a high-resolution source image.
 *
 * Disk-backed sprites have a reliable stream offset and are matched by that
 * offset.  Some sprites (notably the bitmap font glyphs and a few small UI
 * images) are stored inline in the display record: their first word pair is
 * the geometry and their pixel bytes begin at +4.  The DWORD at +8 of those
 * pseudo-records is part of the pixel data, not a stream offset, so matching
 * by offset either misses or can select an unrelated image.  The caller sets
 * bMatchByCrc for those images; the lookup then identifies the extracted
 * counterpart by geometry and the CRC of the actual low-resolution pixels. */
static int nw_asset_get_high_res_sprite_impl(DWORD dwOffset,
                            const unsigned char *pbPixels,
                            int w, int h, int bMatchByCrc,
                            const unsigned char **ppPixels,
                            int *pw, int *ph)
{
    const NwAssetEntry *e = NULL;
    NwAssetHighRes *cache;
    char  szPath[NW_ASSET_PATH_MAX];
    FILE *f;
    BYTE *pb;
    long  cbFile;
    DWORD crc = 0;
    int   i;

    NwAssetOverrideInit();
    if (g_assetOverrideCount == 0)
        return 0;
    if (bMatchByCrc) {
        if (pbPixels == NULL || w <= 0 || h <= 0)
            return 0;
        crc = NwAssetCrc32(pbPixels, (DWORD)w * (DWORD)h);
    }

    for (i = 0; i < g_assetHighResCount; i++) {
        const NwAssetHighRes *entry = &g_assetHighRes[i];
        int hit;
        if (bMatchByCrc)
            hit = entry->dwCrc32 == crc && entry->w == w && entry->h == h;
        else
            hit = entry->dwOffset == dwOffset &&
                  (w <= 0 || (entry->w == w && entry->h == h));
        if (hit) {
            if (ppPixels != NULL)
                *ppPixels = entry->pbPixels;
            if (pw != NULL)
                *pw = (int)entry->sw;
            if (ph != NULL)
                *ph = (int)entry->sh;
            return 1;
        }
    }

    for (i = 0; i < g_assetOverrideCount; i++) {
        const NwAssetEntry *candidate = &g_assetOverrides[i];
        if (candidate->sw == candidate->w && candidate->sh == candidate->h)
            continue;
        if (bMatchByCrc) {
            if (candidate->dwCrc32 == crc &&
                candidate->w == w && candidate->h == h) {
                e = candidate;
                break;
            }
        } else if (candidate->dwOffset == dwOffset &&
                   (w <= 0 || (candidate->w == w && candidate->h == h))) {
            e = candidate;
            break;
        }
    }
    if (e == NULL)
        return 0;

    if (e->szFile[0] == '\0') {
        if (g_assetOverlayPath[0] == '\0')
            return 0;
        f = fopen(g_assetOverlayPath, "rb");
        if (f == NULL)
            return 0;
        if (fseek(f, (long)e->dwDataOffset, SEEK_SET) != 0) {
            fclose(f);
            return 0;
        }
        cbFile = (long)e->dwDataLen;
    } else {
        snprintf(szPath, sizeof szPath, "%s" NW_ASSET_SEP "%s",
                 g_assetOverrideRoot, e->szFile);
        f = fopen(szPath, "rb");
        if (f == NULL)
            return 0;
        if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
        cbFile = ftell(f);
        rewind(f);
    }
    if (cbFile <= 0 || (long)e->sw * (long)e->sh != cbFile) {
        fclose(f);
        return 0;
    }
    pb = (BYTE *)malloc((size_t)cbFile);
    if (pb == NULL) { fclose(f); return 0; }
    if (fread(pb, 1, (size_t)cbFile, f) != (size_t)cbFile) {
        free(pb);
        fclose(f);
        return 0;
    }
    fclose(f);

    cache = nw_asset_high_res_append();
    if (cache == NULL) {
        free(pb);
        return 0;
    }
    cache->dwOffset = bMatchByCrc ? 0 : dwOffset;
    cache->dwCrc32 = crc;
    cache->pbPixels = pb;
    cache->w = e->w;
    cache->h = e->h;
    cache->sw = e->sw;
    cache->sh = e->sh;

    if (ppPixels != NULL)
        *ppPixels = cache->pbPixels;
    if (pw != NULL)
        *pw = (int)cache->sw;
    if (ph != NULL)
        *ph = (int)cache->sh;
    return 1;
}

int NwAssetGetHighResSprite(DWORD dwOffset, const unsigned char *pbPixels,
                            int w, int h, int bMatchByCrc,
                            const unsigned char **ppPixels,
                            int *pw, int *ph)
{
    Uint64 start;
    int result;

    if (!g_assetHighResEnabled)
        return 0;
    start = SDL_GetTicksNS();
    result = nw_asset_get_high_res_sprite_impl(dwOffset, pbPixels, w, h,
                                               bMatchByCrc, ppPixels,
                                               pw, ph);
    g_assetLoadNs += SDL_GetTicksNS() - start;
    g_assetLoadCalls++;
    if (!result)
        g_assetLoadMisses++;
    return result;
}

/* Full-screen background counterpart to NwAssetGetHighResSprite.  The pack
 * stores it under a synthetic offset because the background is not referenced
 * by a Disk sprite descriptor. */
int NwAssetGetHighResBackground(const unsigned char **ppPixels,
                                int *pw, int *ph)
{
    return NwAssetGetHighResSprite(NW_ASSET_BACKGROUND_OFFSET,
                                   NULL, 0, 0, 0, ppPixels, pw, ph);
}

/* The 4x background is a single 4.9 MB image and is the first high-resolution
 * asset a level draws.  Load it during startup so the first gameplay frame
 * does not pay the storage read. */
void NwAssetPreloadHighRes(void)
{
    const unsigned char *pixels = NULL;
    int w = 0;
    int h = 0;
    Uint64 start;
    char stage[160];

    extern int NwSdlRenderScale(void);
    if (NwSdlRenderScale() <= 1)
        return;
    start = SDL_GetTicksNS();
    if (!NwAssetGetHighResBackground(&pixels, &w, &h))
        return;
    snprintf(stage, sizeof stage, "asset_preload_bg_us_%u_%dx%d_p_%llx",
             (unsigned)((SDL_GetTicksNS() - start) / 1000u), w, h,
             (unsigned long long)(uintptr_t)pixels);
    NW_ASSET_LOG(stage);
}

/* A dedicated mapping keeps the ~54 MB high-resolution pool out of the small
 * libc heap and out of the low address range the game's 4x framebuffer and
 * guest pointers still require. */
static void *nw_asset_pool_alloc(size_t cb)
{
#if defined(__unix__) || defined(__APPLE__)
    void *p = mmap(NULL, cb, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
#else
    return malloc(cb);
#endif
}

/* Load every high-resolution entry at startup.  The first level touches only a
 * handful of sprites, but each miss pays a separate storage read; on the
 * console eight such reads were measured at roughly 140 ms.  Read the pack
 * once and keep the pixels for the rest of the session. */
void NwAssetPreloadAllHighRes(void)
{
    FILE *pack = NULL;
    Uint64 start;
    unsigned loaded = 0;
    unsigned failed = 0;
    unsigned long long bytes = 0;
    size_t total = 0;
    size_t used = 0;
    BYTE *pool = NULL;
    int i;
    char stage[192];

    extern int NwSdlRenderScale(void);
    if (NwSdlRenderScale() <= 1)
        return;
    NwAssetOverrideInit();
    if (g_assetOverrideCount == 0)
        return;

    start = SDL_GetTicksNS();

    /* Size one contiguous pool first.  The first version allocated every
     * sprite separately; on the console those 1,183 small blocks fragmented
     * the low address range and the later 4x framebuffer allocation came back
     * with the wrong backing.  A single large allocation is normally served
     * by mmap, so it does not crowd the fixed 32-bit-friendly region the game
     * still needs for its own buffers. */
    for (i = 0; i < g_assetOverrideCount; i++) {
        const NwAssetEntry *e = &g_assetOverrides[i];
        size_t cb;

        if (e->sw == e->w && e->sh == e->h)
            continue;
        if (nw_asset_high_res_has(e->dwOffset, e->dwCrc32, e->w, e->h))
            continue;
        cb = e->dwDataLen ? (size_t)e->dwDataLen
                          : (size_t)e->sw * (size_t)e->sh;
        total += cb;
    }

    if (total != 0) {
        pool = (BYTE *)nw_asset_pool_alloc(total);
        if (pool != NULL && g_assetOverlayPath[0] != '\0')
            pack = fopen(g_assetOverlayPath, "rb");
        if (pool != NULL) {
            snprintf(stage, sizeof stage, "asset_preload_pool_%llx_%u",
                     (unsigned long long)(uintptr_t)pool, (unsigned)total);
            NW_ASSET_LOG(stage);
        }
    }

    for (i = 0; i < g_assetOverrideCount && pool != NULL; i++) {
        const NwAssetEntry *e = &g_assetOverrides[i];
        NwAssetHighRes *cache;
        BYTE *pb;
        FILE *f;
        char path[NW_ASSET_PATH_MAX];
        size_t cb;

        if (e->sw == e->w && e->sh == e->h)
            continue;
        if (nw_asset_high_res_has(e->dwOffset, e->dwCrc32, e->w, e->h))
            continue;
        cb = e->dwDataLen ? (size_t)e->dwDataLen
                          : (size_t)e->sw * (size_t)e->sh;
        if (cb == 0)
            continue;

        pb = pool + used;
        used += cb;

        f = pack;
        if (e->szFile[0] != '\0') {
            int n = snprintf(path, sizeof path, "%s" NW_ASSET_SEP "%s",
                             g_assetOverrideRoot, e->szFile);
            if (n <= 0 || (size_t)n >= sizeof path) {
                failed++;
                continue;
            }
            f = fopen(path, "rb");
            if (f == NULL) {
                failed++;
                continue;
            }
        } else if (f == NULL) {
            failed++;
            continue;
        }
        if (e->szFile[0] == '\0' &&
            fseek(f, (long)e->dwDataOffset, SEEK_SET) != 0) {
            failed++;
            continue;
        }
        if (fread(pb, 1, cb, f) != cb) {
            if (f != pack)
                fclose(f);
            failed++;
            continue;
        }
        if (f != pack)
            fclose(f);

        cache = nw_asset_high_res_append();
        if (cache == NULL)
            break;
        cache->dwOffset = e->dwOffset;
        cache->dwCrc32 = e->dwCrc32;
        cache->pbPixels = pb;
        cache->w = e->w;
        cache->h = e->h;
        cache->sw = e->sw;
        cache->sh = e->sh;
        loaded++;
        bytes += cb;
    }
    if (pack != NULL)
        fclose(pack);

    snprintf(stage, sizeof stage, "asset_preload_all_us_%u_n_%u_b_%u_f_%u",
             (unsigned)((SDL_GetTicksNS() - start) / 1000u),
             loaded, (unsigned)bytes, failed);
    NW_ASSET_LOG(stage);
}

/* The VM stream lives at the front of overlay.nw.  Read it once during startup
 * so the first level's stream walk does not pay cold-storage latency. */
void NwAssetWarmStreamCache(void)
{
#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
    const size_t want = 5u * 1024u * 1024u;
    const size_t chunk = 1u * 1024u * 1024u;
    FILE *f;
    BYTE *buf;
    size_t total = 0;
    size_t got;
    Uint64 start;
    char stage[160];

    if (g_assetOverlayPath[0] == '\0')
        return;
    f = fopen(g_assetOverlayPath, "rb");
    if (f == NULL)
        return;
    buf = (BYTE *)malloc(chunk);
    if (buf == NULL) {
        fclose(f);
        return;
    }
    start = SDL_GetTicksNS();
    while (total < want && (got = fread(buf, 1, chunk, f)) > 0)
        total += got;
    fclose(f);
    free(buf);
    snprintf(stage, sizeof stage, "asset_stream_warm_us_%u_b_%u",
             (unsigned)((SDL_GetTicksNS() - start) / 1000u),
             (unsigned)total);
    NW_ASSET_LOG(stage);
#endif
}

#endif /* NW_PLATFORM_SDL */

BYTE *__stdcall LoadDiskSpriteImageFromStream(WORD *pwSpriteRecord)
{
    BYTE  *pbRecord    = (BYTE *)pwSpriteRecord;
    DWORD  dwTag       = *(DWORD  *)(pbRecord + 0x04);
    DWORD  dwOffset    = *(DWORD  *)(pbRecord + 0x08);
    DWORD  dwStored    = *(DWORD  *)(pbRecord + 0x0c);
    BYTE  *pbPixelBuf  = GLOBAL_SPRITE_PIXEL_BUF;
    unsigned int  uRows;
    unsigned int  uStride;

    if (dwTag != DISK_IMAGE_TAG) {
        return pbRecord + 4;                       
    }

    if (dwOffset != GLOBAL_RESIDENT_OFFSET) {

        GLOBAL_RESIDENT_OFFSET = dwOffset;         
        (void)SeekStreamLogicalOffset(SEEK_BEGIN_STREAM, (int)dwOffset);

        if (dwStored == 0) {

            /* The payload length is NOT dwStored (that is 0 here); it is
               computed from the record's own geometry as rows * stride, the
               same way the caller uses those two words. */
            uRows   = *(unsigned short *)(pbRecord + 0x00);
            uStride = *(unsigned short *)(pbRecord + 0x02);

            (void)ReadExactChunkFromStream(uRows * uStride,
                                           (char *)pbPixelBuf);
        } else {

            /* Staging block from the heap arena first, so the compressed input
               and the decompressed output cannot alias, exactly as in
               src/io/ReadStreamBlockRawOrLzss.c. */
            BYTE *pbBlockEnd = (BYTE *)GetHeapArenaBlockEnd(dwStored);

            (void)ReadExactChunkFromStream(dwStored, (char *)pbBlockEnd);
            DecompressLzssBlockToBuffer((int)dwStored, pbPixelBuf, pbBlockEnd);
        }
    }

#ifdef NW_PLATFORM_SDL
    /* This block is deliberately outside the load-cache arm: a cache hit still
     * needs the dump/override pass, and the Win32 preprocessor output stays
     * byte-for-byte the original function. */
    {
        WORD  w       = *(unsigned short *)(pbRecord + 0x00);
        WORD  h       = *(unsigned short *)(pbRecord + 0x02);
        BYTE *pbPixels = pbPixelBuf + (dwStored == 0 ? 0u : LZSS_IMAGE_HEADER_BYTES);

        if (dwOffset == GLOBAL_RESIDENT_OFFSET) {
            NwAssetDumpSprite(dwOffset, w, h, dwStored, pbPixels);
            NwAssetApplyOverride(dwOffset, w, h, pbPixels);
        }
    }
#endif

    return pbPixelBuf + (dwStored == 0 ? 0u : LZSS_IMAGE_HEADER_BYTES);
}
#endif /* NW_UNIT_LoadDiskSpriteImageFromStream */

/* ==========================================================================
 * Unit: LoadNetwalkTileImageFile
 * Source: video/LoadNetwalkTileImageFile.c
 * ========================================================================== */
#ifdef NW_UNIT_LoadNetwalkTileImageFile
/*
 * Load Netwalk Tile Image File.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-loadnetwalktileimagefile-c
 */

#include <stdint.h>
#include "nwglobals.h"

#if defined(__i386__)
#  define NETWALK_CALL __stdcall
#else
#  define NETWALK_CALL        
#endif

/* ---- callees (bridge) ---------------------------------------------------- */

extern int CallOpenTrackedHandleSlotWithFixedModeBits(
                        const char *pszFilename,
                        uint32_t    dwAccessWord,
                        uint32_t    dwAttrInput);

extern void NETWALK_CALL CloseTrackedHandleSlot(int nTrackedSlotIndex);

extern int __cdecl ReadStreamChunkCrLfNormalized(
                        uint32_t    dwFileIndex,
                        void       *pvDest,
                        uint32_t    dwByteCount);

extern void __stdcall ShowMainWindowErrorBoxWithFixedCaption(const char *lpszText);

extern void NETWALK_CALL SetLogicalPaletteDrawGrid(
                        uint32_t            dwFlags,
                        uint32_t            dwMode,
                        uint32_t            dwPixelCount,
                        const unsigned char *pRgb24);

/* ---- globals ------------------------------------------------------------- */

typedef struct __attribute__((__packed__)) NetwalkImageRecord {
    uint32_t dwReserved[2];   /* +0x00 */
    uint32_t dwTrailingBytes; /* +0x08  appended blob size, also added to bfSize */
    uint32_t dwReserved2;     /* +0x0C */
    uint32_t dwReserved3;     /* +0x10 */
} NetwalkImageRecord;

typedef struct __attribute__((__packed__)) NetwalkTileSignature {
    uint32_t dwReserved[3];   /* +0x00 */
    uint32_t dwTagA;          /* +0x0C */
    uint32_t dwTagB;          /* +0x10 */
} NetwalkTileSignature;

extern const char           g_szImageFileError[];      

#define NETWALK_BM_MAGIC        0x4D42u  /* 'BM' little-endian          */
#define NETWALK_FIXED_HDR_BYTES 0x36u     /* 14 file header + 40 info    */
#define NETWALK_TILE_WIDTH      8u
#define NETWALK_DEFAULT_PIXELS  0x100u   /* used when the count field is 0 */
#define NETWALK_GENERIC_READ    0x8000u

/* ---- locals -------------------------------------------------------------- */

typedef struct __attribute__((__packed__)) NetwalkFileHeader {   /* 14 bytes */
    uint16_t bfType;       /* +0x00  must be 0x4D42                    */
    uint32_t bfSize;       /* +0x02  must be rec->dwTrailingBytes
                                   + 0x36 + n*4                       */
    uint16_t bfReserved1;  /* +0x06  must be 0                        */
    uint16_t bfReserved2;  /* +0x08  must be 0                        */
    uint32_t bfOffBits;    /* +0x0A  must be 0x36 + n*4               */
} NetwalkFileHeader;

typedef struct __attribute__((__packed__)) NetwalkTileInfo {     /* 40 bytes */
    uint32_t biSize;          /* +0x00                                    */
    uint32_t biWidth;         /* +0x04  must equal signature->dwTagA      */
    uint32_t biHeight;        /* +0x08  must equal signature->dwTagB      */
    uint16_t biPlanes;        /* +0x0C  must be 1                         */
    uint16_t biBitCount;      /* +0x0E  must be 8                         */
    uint32_t biCompression;   /* +0x10  must be 0                         */
    uint32_t biSizeImage;     /* +0x14                                    */
    uint32_t biXPelsPerMeter; /* +0x18                                    */
    uint32_t biYPelsPerMeter; /* +0x1C                                    */
    uint32_t biClrUsed;       /* +0x20  used as a PIXEL COUNT             */
    uint32_t biClrImportant;  /* +0x24                                    */
} NetwalkTileInfo;

/* ------------------------------------------------------------------------- */

void NETWALK_CALL LoadNetwalkTileImageFile(const char *pszFilename)
{
    NetwalkFileHeader FileHeader;
    NetwalkTileInfo   Info;
    unsigned char     PixelData[NETWALK_DEFAULT_PIXELS * 4];
    unsigned char     ExpandedRgb[NETWALK_DEFAULT_PIXELS * 3];

    uint32_t dwPixelCount;
    uint32_t dwPixelBytes;
    uint32_t dwExpectedSize;
    uint32_t i;
    int      nTrackedSlot;

    nTrackedSlot = CallOpenTrackedHandleSlotWithFixedModeBits(
                       pszFilename, NETWALK_GENERIC_READ, 0u);

    if (nTrackedSlot == -1) {
        /* Failed open: straight to the epilogue.  No error box, no close. */
        return;
    }

    (void)ReadStreamChunkCrLfNormalized((uint32_t)nTrackedSlot, &FileHeader,
                                        (uint32_t)sizeof(FileHeader));
    (void)ReadStreamChunkCrLfNormalized((uint32_t)nTrackedSlot, &Info,
                                        (uint32_t)sizeof(Info));

    dwPixelCount = Info.biClrUsed;
    if (dwPixelCount == 0u) {
        dwPixelCount = NETWALK_DEFAULT_PIXELS;
    }
    dwPixelBytes = dwPixelCount * 4u;

    (void)ReadStreamChunkCrLfNormalized((uint32_t)nTrackedSlot, PixelData,
                                        dwPixelBytes);

    dwExpectedSize = g_pNetwalkImageRecord->dwTrailingBytes
                   + NETWALK_FIXED_HDR_BYTES + dwPixelBytes;

    if (FileHeader.bfType != NETWALK_BM_MAGIC)                     goto reject;
    if (FileHeader.bfSize != dwExpectedSize)                       goto reject;
    if (FileHeader.bfReserved1 != 0u)                              goto reject;
    if (FileHeader.bfReserved2 != 0u)                              goto reject;
    if (FileHeader.bfOffBits != NETWALK_FIXED_HDR_BYTES + dwPixelBytes) goto reject;
    if (Info.biPlanes != 1u)                                       goto reject;
    if (Info.biBitCount != 8u)                                     goto reject;
    if (Info.biCompression != 0u)                                  goto reject;
    if (Info.biWidth != g_pNetwalkTileSignature->dwTagA)           goto reject;
    if (Info.biHeight != g_pNetwalkTileSignature->dwTagB)          goto reject;

    /* Uncompress: 32-bit 0x00RRGGBB dwords -> packed 24-bit RGB triplets.
     * The loop stores src[2],src[1],src[0] at ascending destinations. */
    for (i = 0u; i < dwPixelCount; i++) {
        ExpandedRgb[i * 3u + 0u] = PixelData[i * 4u + 2u];
        ExpandedRgb[i * 3u + 1u] = PixelData[i * 4u + 1u];
        ExpandedRgb[i * 3u + 2u] = PixelData[i * 4u + 0u];
    }

    SetLogicalPaletteDrawGrid(1u, 0u, dwPixelCount, ExpandedRgb);

    /* Trailing blob tacked on after the pixel array. */
    (void)ReadStreamChunkCrLfNormalized(
        (uint32_t)nTrackedSlot,
        (void *)(uintptr_t)g_pNetwalkImageRecord->dwReserved2,
        g_pNetwalkImageRecord->dwTrailingBytes);

    CloseTrackedHandleSlot(nTrackedSlot);
    return;

reject:
    ShowMainWindowErrorBoxWithFixedCaption(g_szImageFileError);
    CloseTrackedHandleSlot(nTrackedSlot);
}
#endif /* NW_UNIT_LoadNetwalkTileImageFile */

/* ==========================================================================
 * Unit: PublishScreenGeometryAndClaimFrameBuffer
 * Source: video/PublishScreenGeometryAndClaimFrameBuffer.c
 * ========================================================================== */
#ifdef NW_UNIT_PublishScreenGeometryAndClaimFrameBuffer
/*
 * Publishes the framebuffer pitch, row count and base pixel address to the three screen globals.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-publishscreengeometryandclaimframebuffer-c
 */

#include <windows.h>
#include <stdint.h>
extern unsigned char nw_data_window[];

/* @(nw_data_window + 0x6258) - the screen/source-surface descriptor. */
typedef struct ScreenDescriptor {
    DWORD dwMagicTag;      
    DWORD dwBlockCountKb;  
    DWORD dwReserved08;    /* +0x08 - not read by this function                          */
    DWORD dwPitch;         /* +0x0c - BYTES PER ROW; published to (nw_data_window + 0x1ccc)            */
    DWORD dwRows;          /* +0x10 - ROW COUNT;        published to (nw_data_window + 0x1cd0)            */
} ScreenDescriptor;

typedef struct FrameBufferArena {
    DWORD    dwBytesRemaining; /* +0x00 - budget; initialised by the caller from
                                *         dwBlockCountKb << 10.                        */
    uint32_t pBumpCursor;      /* +0x04 - 32-bit guest cursor into the framebuffer band */
    DWORD    dwBufferBytes;    /* +0x08 - pitch * rows, written by the caller          */
    uint32_t pCachedBase;      /* +0x0c - cached guest base; 0 means "not claimed yet" */
} FrameBufferArena;

#define g_pFrameBufferBits NW_WINDOW_PTR_VALUE(0x1cc8) /* base pixel address */
#define g_dwFramePitch     (*(DWORD *)(nw_data_window + 0x1ccc))     /* bytes per row      */
#define g_dwFrameRows      (*(DWORD *)(nw_data_window + 0x1cd0))     /* row count          */

int __stdcall PublishScreenGeometryAndClaimFrameBuffer(
    const ScreenDescriptor *pDescriptor, /* (nw_data_window + 0x6258) in the known caller */
    FrameBufferArena       *pArena)      /* (nw_data_window + 0x6314) in the known caller */
{
#ifdef NW_PLATFORM_SDL
    {
        /* The SDL backend owns the framebuffer as a native allocation; the
         * 32-bit cached-base field in the arena cannot carry a 64-bit host
         * pointer.  Always publish the side-table DIB pointer and derive the
         * physical pitch/rows from the current render scale. */
        extern int NwSdlRenderScale(void);
        int scale = NwSdlRenderScale();
        if (scale < 1)
            scale = 1;

        g_dwFramePitch = pDescriptor->dwPitch * (DWORD)scale;
        g_dwFrameRows  = pDescriptor->dwRows * (DWORD)scale;
        g_pFrameBufferBits = NW_WINDOW_PTR_VALUE(0x6060);
        return 1;
    }
#endif

    /* Two unconditional publishes from the descriptor's +0xc and +0x10 fields. */
    g_dwFramePitch = pDescriptor->dwPitch;
    g_dwFrameRows  = pDescriptor->dwRows;

    g_pFrameBufferBits = (uintptr_t)pArena->pCachedBase;

    if (pArena->pCachedBase == 0) {
        /* Fallback: first use. Claim at the cursor, memoize, republish.
         * uint32_t throughout, because the asm's ADD/SUB are 32-bit and would
         * wrap at 2^32 - a 64-bit intermediate would not wrap the same way. */
        uint32_t uClaimed = (uint32_t)pArena->pBumpCursor;

        pArena->pCachedBase      = uClaimed;
        g_pFrameBufferBits       = (uintptr_t)uClaimed;
        pArena->pBumpCursor      = (uint32_t)(uClaimed + pArena->dwBufferBytes);
        pArena->dwBytesRemaining = pArena->dwBytesRemaining - pArena->dwBufferBytes;
    }

    return 1; 
}
#endif /* NW_UNIT_PublishScreenGeometryAndClaimFrameBuffer */

/* ==========================================================================
 * Unit: TryPlayStartupMovieWithWindowFallback
 * Source: video/TryPlayStartupMovieWithWindowFallback.c
 * ========================================================================== */
#ifdef NW_UNIT_TryPlayStartupMovieWithWindowFallback
/*
 * Plays the startup movie and falls back to a timed intro.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-tryplaystartupmoviewithwindowfallback-c
 */

#include <stdint.h>
#include "nwglobals.h"

typedef unsigned long  DWORD;
typedef unsigned char  BYTE;
typedef int            BOOL;

#define TRUE  1
#define FALSE 0

/* ---- state block ------------------------------------------------------ */
extern DWORD g_dwShowCommand;   
extern DWORD g_dwStateLatch;    
                                /*              SetStateFlagOnce              */
extern DWORD g_dwRequiredMin0;  
extern DWORD g_dwRequiredMin1;  
extern DWORD g_dwCurrent0;      
extern DWORD g_hwndMain;        

typedef BOOL (__stdcall *PFN_ShowWindow)(DWORD, int);              
typedef BOOL (__stdcall *PFN_UpdateWindow)(DWORD);                 
typedef DWORD (__stdcall *PFN_SendMessageA)(DWORD, DWORD, DWORD, DWORD); 

extern PFN_ShowWindow    pfnShowWindow;
extern PFN_UpdateWindow  pfnUpdateWindow;
extern PFN_SendMessageA  pfnSendMessageA;

/* ---- callees ---------------------------------------------------------- */
extern void   __cdecl DispatchQueuedNonInputMessages(void);          
extern BOOL   __stdcall PlayStartupMovieWithMessagePumpAndTeardown(
                  DWORD dwMovieRectWord3,   
                  DWORD dwMovieRectWord2,   
                  DWORD dwMovieWindowY,     
                  DWORD dwMovieWindowX,     
                  char *pszExtraArgument);  /* arg5, supplied as 0     */
extern DWORD  __stdcall SetStateFlagOnce(DWORD dwValue);             

extern const BYTE g_failureMarker[1];   

/* ---- the tile record whose +0x28..+0x34 become the movie arguments ---- */
typedef struct TileRecord_s {
    BYTE  bPad00[0x28];
    DWORD dwMovieWindowX;      /* +0x28 -> callee arg4 */
    DWORD dwMovieWindowY;      /* +0x2C -> callee arg3 */
    DWORD dwMovieRectWord2;    /* +0x30 -> callee arg2 */
    DWORD dwMovieRectWord3;    /* +0x34 -> callee arg1 */
} TileRecord;

/* =================================================================== */

BOOL TryPlayStartupMovieWithWindowFallback(void)
{
#ifdef NW_PLATFORM_SDL
    /* The startup movie is deferred for the portable backend.  Windows keeps
     * the unchanged MCI path; the SDL build goes straight to the game screen so
     * no Win32 movie window or modal message pump is needed.  The first game
     * frame rebuilds the palette and marks its own dirty extent, so no movie
     * teardown state is required. */
    extern void nwtrace_hex(const char *, unsigned long);
    nwtrace_hex("tpsm_skipped", 1);
    return TRUE;
#else
    TileRecord *pRecord;
    BOOL        bMoviePlayed;
    extern void nwtrace_hex(const char *, unsigned long);

    nwtrace_hex("tpsm_cur0", (unsigned long)g_dwCurrent0);
    nwtrace_hex("tpsm_req0", (unsigned long)g_dwRequiredMin0);
    nwtrace_hex("tpsm_cur1", (unsigned long)g_dwCurrent1);
    nwtrace_hex("tpsm_req1", (unsigned long)g_dwRequiredMin1);

    if (g_dwCurrent0 < g_dwRequiredMin0) {
        goto Failed;
    }
    if (g_dwCurrent1 < g_dwRequiredMin1) {
        goto Failed;
    }
    nwtrace_hex("tpsm_guard", 1);

    DispatchQueuedNonInputMessages();

    pRecord      = (TileRecord *)(uintptr_t)g_pdwTileRecord;
    bMoviePlayed = PlayStartupMovieWithMessagePumpAndTeardown(
                       pRecord->dwMovieRectWord3,      /* arg1: [EAX+0x34] */
                       pRecord->dwMovieRectWord2,      /* arg2: [EAX+0x30] */
                       pRecord->dwMovieWindowY,        /* arg3: [EAX+0x2C] */
                       pRecord->dwMovieWindowX,        /* arg4: [EAX+0x28] */
                       (char *)0);                     /* arg5: PUSH 0x0    */
    nwtrace_hex("tpsm_movie", (unsigned long)bMoviePlayed);

    if (bMoviePlayed) {
        goto Done;                  
    }
    nwtrace_hex("tpsm_fallback", 1);

    /* nCmdShow is read from a global; its value is not in these bytes.      */
    (void)pfnShowWindow(g_hwndMain, (int)g_dwShowCommand);

    (void)pfnUpdateWindow(g_hwndMain);

    /* 0x30F = WM_USER + 0x0F; both wParam and lParam are immediate 0.     */
    (void)pfnSendMessageA(g_hwndMain, 0x30FUL, 0UL, 0UL);

Done:

    return TRUE;

Failed:

    nwtrace_hex("tpsm_failed", 1);
    SetStateFlagOnce((DWORD)(uintptr_t)g_failureMarker);
    return FALSE;
#endif /* NW_PLATFORM_SDL */
}
#endif /* NW_UNIT_TryPlayStartupMovieWithWindowFallback */
