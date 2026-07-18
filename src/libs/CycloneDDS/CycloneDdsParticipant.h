#ifndef CYCLONEDDS_CYCLONEDDSPARTICIPANT_H_
#define CYCLONEDDS_CYCLONEDDSPARTICIPANT_H_

#include <dds/dds.h>

#include <cstdint>

namespace CycloneDDS
{

/**
 * @brief Owns the lifecycle of a DDS participant plus one subscriber and
 *        one publisher entity on it (via the CycloneDDS C API).
 *
 * This is the one piece of DDS entity plumbing shared by the raw
 * type-erased discovery/subscribe/publish classes in this module — they
 * are constructed against an existing CycloneDdsParticipant rather than
 * each creating their own participant.
 */
class CycloneDdsParticipant
{
public:
   explicit CycloneDdsParticipant(uint32_t domainId);
   ~CycloneDdsParticipant();

   CycloneDdsParticipant(const CycloneDdsParticipant &) = delete;
   CycloneDdsParticipant &operator=(const CycloneDdsParticipant &) = delete;

   /// True if the participant (and its subscriber/publisher) were created
   /// successfully.
   [[nodiscard]] bool isValid() const { return _participant > 0; }

   [[nodiscard]] dds_entity_t participant() const { return _participant; }
   [[nodiscard]] dds_entity_t subscriber() const { return _subscriber; }
   [[nodiscard]] dds_entity_t publisher() const { return _publisher; }

private:
   dds_entity_t _participant{0};
   dds_entity_t _subscriber{0};
   dds_entity_t _publisher{0};
};

} // namespace CycloneDDS

#endif // CYCLONEDDS_CYCLONEDDSPARTICIPANT_H_
