#include "CycloneDDS/DDSTopicConfig.h"

namespace CycloneDDS
{

DDSTopicConfig::DDSTopicConfig(std::initializer_list<TopicEntry> entries)
{
   for (const auto &e : entries)
   {
      _entries.emplace(e.topicName, e);
   }
}

DDSTopicConfig::DDSTopicConfig(const std::vector<TopicEntry> &entries)
{
   for (const auto &e : entries)
   {
      _entries.emplace(e.topicName, e);
   }
}

const TopicEntry &DDSTopicConfig::getEntry(const std::string &topicName) const
{
   auto it = _entries.find(topicName);
   if (it == _entries.end())
   {
      throw std::out_of_range(
         "DDSTopicConfig: topic '" + topicName + "' is not registered");
   }
   return it->second;
}

const dds::pub::qos::DataWriterQos &
DDSTopicConfig::writerQos(const std::string &topicName) const
{
   return getEntry(topicName).writerQos;
}

const dds::sub::qos::DataReaderQos &
DDSTopicConfig::readerQos(const std::string &topicName) const
{
   return getEntry(topicName).readerQos;
}

bool DDSTopicConfig::hasTopic(const std::string &topicName) const
{
   return _entries.contains(topicName);
}

std::vector<std::string> DDSTopicConfig::topicNames() const
{
   std::vector<std::string> names;
   names.reserve(_entries.size());
   for (const auto &[name, _] : _entries)
   {
      names.push_back(name);
   }
   return names;
}

} // namespace CycloneDDS
