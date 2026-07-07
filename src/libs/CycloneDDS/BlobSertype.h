#ifndef BLOBSERTYPE_H_
#define BLOBSERTYPE_H_

#include <dds/dds.h>
#include <dds/ddsi/ddsi_sertype.h>
#include <dds/ddsi/ddsi_serdata.h>

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates a raw blob sertype for the named type.
 *
 * The blob sertype accepts any incoming CDR data without type-checking,
 * storing it as raw bytes. Use dds_create_topic_sertype() to register a
 * topic with this sertype — the domain takes ownership on success.
 *
 * For reading, use dds_takecdr() to obtain ddsi_serdata blobs, then
 * BlobSerdataBytes() to extract the raw CDR buffer.
 *
 * @param typeName  DDS type name string (copied internally).
 * @return Heap-allocated ddsi_sertype, or NULL on allocation failure.
 */
struct ddsi_sertype *blobSertypeCreate(const char *typeName);

/**
 * @brief Returns a pointer to the raw CDR bytes stored in a blob serdata.
 *
 * @param sd        A ddsi_serdata obtained from dds_takecdr().
 * @param sizeOut  Set to the byte count of the payload.
 * @return Pointer into the serdata's internal buffer — valid until
 *         ddsi_serdata_unref(sd) is called.
 */
const uint8_t *blobSerdataBytes(const struct ddsi_serdata *sd, uint32_t *sizeOut);

#ifdef __cplusplus
}
#endif

#endif // BLOBSERTYPE_H_
