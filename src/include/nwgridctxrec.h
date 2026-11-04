/*
 * Grid-context and cell-span record layouts.
 *
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#include-nwgridctxrec-h
 */

#ifndef NWGRIDCTXREC_H
#define NWGRIDCTXREC_H

#pragma pack(push, 1)
typedef struct GridCtxRec {
    union {                                   /* +0x00 */
        uint8_t bStatusFlags;                 /* mode nibble + record class   */
        uint8_t bCtxFlags;                    /* OR with 0x04 = ref-relative  */
    };
    uint8_t  bAux;                            /* +0x01 */
    union {                                   /* +0x02 */
        uint8_t  bPendingIndex;               /* 0xFE = none pending          */
        uint8_t  bStampLow;                   /* stamp word, low              */
    };
    union {                                   /* +0x03 */
        uint8_t  bDispatchFlag;               /* the dispatch flag            */
        uint8_t  bStampHigh;                  /* stamp word, high             */
        uint8_t  bLinkFlags;                  /* bit1 = group pending         */
    };
    uint8_t  bLayerFlags;                     /* +0x04 mask predicates, not bLayer */
    union {                                   /* +0x05 */
        uint8_t  bLayer;                      
        uint8_t  bLinkSrc;                    
    };
    union {                                   /* +0x06 .. +0x07 */
        uint16_t wCellStamp;                  /* MOVZX word at +0x06          */
        struct {
            uint8_t  bCellKeyLow;             
            uint8_t  bCellKeyHigh;            
        };
    };
    union {                                   /* +0x08 */
        uint16_t wCurXIdx;                    
        uint16_t wDepthIndexX;                
    };
    union {                                   /* +0x0A */
        uint16_t wCurYIdx;                    
        uint16_t wDepthIndexY;                
    };
    uint8_t  _pad0C[4];                       /* +0x0C .. +0x0F */
} GridCtxRec;
#pragma pack(pop)

#include <stddef.h>
_Static_assert(offsetof(GridCtxRec, bLinkFlags) == 0x03,
               "GridCtxRec.bLinkFlags must sit at +0x03");
_Static_assert(offsetof(GridCtxRec, bLayerFlags) == 0x04,
               "GridCtxRec.bLayerFlags must sit at +0x04");
_Static_assert(offsetof(GridCtxRec, bLayer) == 0x05,
               "GridCtxRec.bLayer must sit at +0x05");
_Static_assert(offsetof(GridCtxRec, wCellStamp) == 0x06,
               "GridCtxRec.wCellStamp must sit at +0x06");
_Static_assert(offsetof(GridCtxRec, wCurXIdx) == 0x08,
               "GridCtxRec.wCurXIdx must sit at +0x08");
_Static_assert(offsetof(GridCtxRec, wCurYIdx) == 0x0A,
               "GridCtxRec.wCurYIdx must sit at +0x0A");

#endif /* NWGRIDCTXREC_H */
