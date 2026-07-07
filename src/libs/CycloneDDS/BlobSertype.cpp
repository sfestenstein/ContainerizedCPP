#include "CycloneDDS/BlobSertype.h"

#include <dds/ddsi/ddsi_serdata.h>
#include <dds/ddsi/ddsi_sertype.h>
#include <dds/ddsi/q_radmin.h>
#include <dds/ddsc/dds_public_impl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
//  Blob serdata — stores raw CDR bytes from the wire
// ============================================================================

struct BlobSerdata
{
   struct ddsi_serdata c; // must be first (same address as this)
   uint32_t            size;
   uint8_t            *data;
};

// Struct-embedding casts: blob_serdata starts with ddsi_serdata at offset 0.
// Standard-layout guarantees they share the same address, so reinterpret_cast
// is both safe and required here (static_cast doesn't work without inheritance).
static inline BlobSerdata *       toBlob(struct ddsi_serdata *d)       { return reinterpret_cast<BlobSerdata *>(d); }
static inline const BlobSerdata * toBlob(const struct ddsi_serdata *d) { return reinterpret_cast<const BlobSerdata *>(d); }

// ----- serdata helpers -----------------------------------------------------

static BlobSerdata *blobSerdataAlloc(const struct ddsi_sertype *type,
                                         enum ddsi_serdata_kind kind,
                                         uint32_t size)
{
   auto *sd = static_cast<BlobSerdata *>(std::calloc(1, sizeof(BlobSerdata)));
   if (!sd) return nullptr;
   ddsi_serdata_init(&sd->c, type, kind);
   sd->size = size;
   sd->data = size > 0 ? static_cast<uint8_t *>(std::malloc(size)) : nullptr;
   return sd;
}

// ----- serdata ops ---------------------------------------------------------

static bool blobEqKey(const struct ddsi_serdata * /*a*/, const struct ddsi_serdata * /*b*/)
{
   return true; // no key: all instances are identical
}

static uint32_t blobGetSize(const struct ddsi_serdata *d)
{
   return toBlob(d)->size;
}

static struct ddsi_serdata *blobFromSer(const struct ddsi_sertype *type,
                                        enum ddsi_serdata_kind kind,
                                        const struct nn_rdata *fragchain,
                                        size_t size)
{
   BlobSerdata *sd = blobSerdataAlloc(type, kind, static_cast<uint32_t>(size));
   if (!sd) return nullptr;

   // Copy each fragment into the contiguous output buffer.
   // frag->min/maxp1 are byte offsets; the payload bytes are in frag->rmsg.
   for (const struct nn_rdata *frag = fragchain; frag; frag = frag->nextfrag)
   {
      const uint8_t *src = NN_RMSG_PAYLOADOFF(frag->rmsg, NN_RDATA_PAYLOAD_OFF(frag));
      //NOLINTNEXTLINE
      std::memcpy(sd->data + frag->min, src, frag->maxp1 - frag->min);
   }
   return &sd->c;
}

static struct ddsi_serdata *blobFromSerIov(const struct ddsi_sertype *type,
                                               enum ddsi_serdata_kind kind,
                                               ddsrt_msg_iovlen_t niov,
                                               const ddsrt_iovec_t *iov,
                                               size_t size)
{
   BlobSerdata *sd = blobSerdataAlloc(type, kind, static_cast<uint32_t>(size));
   if (!sd) return nullptr;

   uint8_t *dst = sd->data;
   for (ddsrt_msg_iovlen_t i = 0; i < niov; ++i)
   {
      //NOLINTNEXTLINE
      std::memcpy(dst, iov[i].iov_base, iov[i].iov_len);
      dst += iov[i].iov_len;
   }
   return &sd->c;
}

static struct ddsi_serdata *blobFromKeyhash(const struct ddsi_sertype *type,
                                               const struct ddsi_keyhash * /*keyhash*/)
{
   // Return an empty key-kind blob — we don't decode the key, but the instance
   // handle must be non-null so CycloneDDS can register the instance.
   BlobSerdata *sd = blobSerdataAlloc(type, SDK_KEY, 0);
   return sd ? &sd->c : nullptr;
}

static struct ddsi_serdata *blobFromSample(const struct ddsi_sertype * /*type*/,
                                              enum ddsi_serdata_kind /*kind*/,
                                              const void * /*sample*/)
{
   return nullptr;
}

static void blobToSer(const struct ddsi_serdata *d, size_t off, size_t sz, void *buf)
{
   std::memcpy(buf, toBlob(d)->data + off, sz);
}

static struct ddsi_serdata *blobToSerRef(const struct ddsi_serdata *d,
                                             size_t off, size_t sz,
                                             ddsrt_iovec_t *ref)
{
   ref->iov_base = toBlob(d)->data + off;
   ref->iov_len  = sz;
   return ddsi_serdata_ref(d);
}

static void blobToSerUnref(struct ddsi_serdata *d, const ddsrt_iovec_t * /*ref*/)
{
   ddsi_serdata_unref(d);
}

static bool blobToSample(const struct ddsi_serdata * /*d*/,
                            void * /*sample*/, void ** /*bufptr*/, void * /*buflim*/)
{
   return false;
}

static struct ddsi_serdata *blobToUntyped(const struct ddsi_serdata *d)
{
   return ddsi_serdata_ref(d);
}

static bool blobUntypedToSample(const struct ddsi_sertype * /*type*/,
                                    const struct ddsi_serdata * /*d*/,
                                    void * /*sample*/, void ** /*bufptr*/, void * /*buflim*/)
{
   return false;
}

static void blobFreeSd(struct ddsi_serdata *d)
{
   BlobSerdata *sd = toBlob(d);
   std::free(sd->data);
   std::free(sd);
}

static size_t blobPrint(const struct ddsi_sertype * /*type*/,
                          const struct ddsi_serdata *d,
                          char *buf, size_t bufsize)
{
   return static_cast<size_t>(
      std::snprintf(buf, bufsize, "<blob:%u bytes>", toBlob(d)->size));
}

static void blobGetKeyhash(const struct ddsi_serdata * /*d*/,
                              struct ddsi_keyhash * /*buf*/, bool /*force_md5*/) {}

static const struct ddsi_serdata_ops BLOB_SERDATA_OPS = {
   .eqkey             = blobEqKey,
   .get_size          = blobGetSize,
   .from_ser          = blobFromSer,
   .from_ser_iov      = blobFromSerIov,
   .from_keyhash      = blobFromKeyhash,
   .from_sample       = blobFromSample,
   .to_ser            = blobToSer,
   .to_ser_ref        = blobToSerRef,
   .to_ser_unref      = blobToSerUnref,
   .to_sample         = blobToSample,
   .to_untyped        = blobToUntyped,
   .untyped_to_sample = blobUntypedToSample,
   .free              = blobFreeSd,
   .print             = blobPrint,
   .get_keyhash       = blobGetKeyhash,
   .get_sample_size   = nullptr,
   .from_iox_buffer   = nullptr,
};

// ============================================================================
//  Blob sertype
// ============================================================================

struct BlobSertype
{
   struct ddsi_sertype c; // must be first
};

static inline BlobSertype *       toBst(struct ddsi_sertype *t)       { return reinterpret_cast<BlobSertype *>(t); }
static inline const BlobSertype * toBst(const struct ddsi_sertype *t) { return reinterpret_cast<const BlobSertype *>(t); }

// ----- sertype ops ---------------------------------------------------------

static void blobStFree(struct ddsi_sertype *tp)
{
   ddsi_sertype_fini(tp);
   std::free(toBst(tp));
}

static void blobStZeroSamples(const struct ddsi_sertype * /*tp*/,
                                  void * /*samples*/, size_t /*count*/) {}

static void blobStReallocSamples(void **ptrs, const struct ddsi_sertype * /*tp*/,
                                     void * /*old*/, size_t /*oldcount*/, size_t count)
{
   for (size_t i = 0; i < count; ++i) ptrs[i] = nullptr;
}

static void blobStFreeSamples(const struct ddsi_sertype * /*tp*/,
                                  void **ptrs, size_t count, dds_free_op_t op)
{
   if ((op & DDS_FREE_ALL_BIT) != 0)
      for (size_t i = 0; i < count; ++i)
         std::free(ptrs[i]);
}

static bool blobStEqual(const struct ddsi_sertype *a, const struct ddsi_sertype *b)
{
   return std::strcmp(a->type_name, b->type_name) == 0;
}

static uint32_t blobStHash(const struct ddsi_sertype *tp)
{
   uint32_t h = 2166136261u;
   for (const char *c = tp->type_name; *c != '\0'; ++c)
      h = (h ^ static_cast<uint8_t>(*c)) * 16777619u;
   return h;
}

static struct ddsi_sertype *blobStDerive(const struct ddsi_sertype *sertype,
                                         dds_data_representation_id_t /*data_rep*/,
                                         dds_type_consistency_enforcement_qospolicy_t /*tce*/)
{
   return ddsi_sertype_ref(sertype);
}

// Designated initializers must appear in struct-declaration order.
// Order: version, arg, free, zero_samples, realloc_samples, free_samples,
//        equal, hash, type_id, type_map, type_info, derive_sertype,
//        get_serialized_size, serialize_into
static const struct ddsi_sertype_ops BLOB_SERTYPE_OPS = {
   .version             = ddsi_sertype_v0,
   .arg                 = nullptr,
   .free                = blobStFree,
   .zero_samples        = blobStZeroSamples,
   .realloc_samples     = blobStReallocSamples,
   .free_samples        = blobStFreeSamples,
   .equal               = blobStEqual,
   .hash                = blobStHash,
   .type_id             = nullptr,
   .type_map            = nullptr,
   .type_info           = nullptr,
   .derive_sertype      = blobStDerive,
   .get_serialized_size = nullptr,
   .serialize_into      = nullptr,
};

// ============================================================================
//  Public API
// ============================================================================
struct ddsi_sertype *blobSertypeCreate(const char *typeName)
{
   auto *st = static_cast<BlobSertype *>(std::calloc(1, sizeof(BlobSertype)));
   if (!st) return nullptr;

   // Use WITH_KEY (topickind_no_key=false) so we can match publishers that
   // have @key fields. A NO_KEY reader won't be matched by WITH_KEY writers.
   ddsi_sertype_init(&st->c, typeName,
                     &BLOB_SERTYPE_OPS, &BLOB_SERDATA_OPS,
                     /* topickind_no_key */ false);

   st->c.allowed_data_representation =
      DDS_DATA_REPRESENTATION_FLAG_XCDR1 | DDS_DATA_REPRESENTATION_FLAG_XCDR2;

   return &st->c;
}

const uint8_t *blobSerdataBytes(const struct ddsi_serdata *sd, uint32_t *sizeOut)
{
   const BlobSerdata *bd = toBlob(sd);
   if (sizeOut) *sizeOut = bd->size;
   return bd->data;
}
