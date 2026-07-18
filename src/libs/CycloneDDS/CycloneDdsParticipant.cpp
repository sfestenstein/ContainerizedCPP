#include "CycloneDDS/CycloneDdsParticipant.h"

#include "CommonUtils/GeneralLogger.h"

namespace CycloneDDS
{

CycloneDdsParticipant::CycloneDdsParticipant(uint32_t domainId)
{
   _participant = dds_create_participant(
      static_cast<dds_domainid_t>(domainId), nullptr, nullptr);

   if (_participant < 0)
   {
      GPERROR("CycloneDdsParticipant: failed to create participant on domain {} ({})",
              domainId, _participant);
      return;
   }

   _subscriber = dds_create_subscriber(_participant, nullptr, nullptr);
   _publisher  = dds_create_publisher(_participant, nullptr, nullptr);
   if (_publisher < 0)
   {
      GPERROR("CycloneDdsParticipant: failed to create publisher on domain {} ({}) — "
              "publishing will not work", domainId, _publisher);
   }

   GPINFO("CycloneDdsParticipant: joined domain {}", domainId);
}

CycloneDdsParticipant::~CycloneDdsParticipant()
{
   if (_publisher > 0) dds_delete(_publisher);
   if (_subscriber > 0) dds_delete(_subscriber);
   if (_participant > 0) dds_delete(_participant);
}

} // namespace CycloneDDS
