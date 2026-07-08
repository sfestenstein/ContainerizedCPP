#include "FastDDS/FastDDSTopicConfig.h"

#include <stdexcept>

namespace FastDDS
{

FastDDSTopicConfig::FastDDSTopicConfig(std::initializer_list<TopicEntry> entries)
{
   for (const auto &e : entries)
   {
      _entries.emplace(e.topicName, e);
   }
}

FastDDSTopicConfig::FastDDSTopicConfig(const std::vector<TopicEntry> &entries)
{
   for (const auto &e : entries)
   {
      _entries.emplace(e.topicName, e);
   }
}

const TopicEntry &FastDDSTopicConfig::getEntry(const std::string &topicName) const
{
   auto it = _entries.find(topicName);
   if (it == _entries.end())
   {
      throw std::out_of_range(
         "FastDDSTopicConfig: topic '" + topicName + "' is not registered");
   }
   return it->second;
}

const eprosima::fastdds::dds::DataWriterQos &
FastDDSTopicConfig::writerQos(const std::string &topicName) const
{
   return getEntry(topicName).writerQos;
}

const eprosima::fastdds::dds::DataReaderQos &
FastDDSTopicConfig::readerQos(const std::string &topicName) const
{
   return getEntry(topicName).readerQos;
}

bool FastDDSTopicConfig::hasTopic(const std::string &topicName) const
{
   return _entries.contains(topicName);
}

std::vector<std::string> FastDDSTopicConfig::topicNames() const
{
   std::vector<std::string> names;
   names.reserve(_entries.size());
   for (const auto &[name, _] : _entries)
   {
      names.push_back(name);
   }
   return names;
}

} // namespace FastDDS
