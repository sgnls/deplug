#include <nan.h>
#include <plugkit/dissector.hpp>
#include <plugkit/stream_dissector.hpp>
#include <plugkit/layer.hpp>
#include <plugkit/property.hpp>
#include <plugkit/fmt.hpp>
#include <unordered_map>

using namespace plugkit;

class IPv4Dissector final : public Dissector {
public:
  class Worker final : public Dissector::Worker {
  public:
    Layer *analyze(Layer *layer) override {
      fmt::Reader<Slice> reader(layer->payload());
      Layer *child = new Layer(MNS("ipv4"));

      uint8_t header = reader.readBE<uint8_t>();
      int version = header >> 4;
      int headerLength = header & 0b00001111;

      Property *ver = new Property(MID("version"), version);
      ver->setRange(reader.lastRange());
      ver->setError(reader.lastError());
      child->addProperty(ver);

      Property *hlen = new Property(MID("hLen"), headerLength);
      hlen->setRange(reader.lastRange());
      hlen->setError(reader.lastError());
      child->addProperty(hlen);

      Property *tos = new Property(MID("type"), reader.readBE<uint8_t>());
      tos->setRange(reader.lastRange());
      tos->setError(reader.lastError());
      child->addProperty(tos);

      uint16_t totalLength = reader.readBE<uint16_t>();
      Property *tlen = new Property(MID("tLen"), totalLength);
      tlen->setRange(reader.lastRange());
      tlen->setError(reader.lastError());
      child->addProperty(tlen);

      Property *id = new Property(MID("id"), reader.readBE<uint16_t>());
      id->setRange(reader.lastRange());
      id->setError(reader.lastError());
      child->addProperty(id);

      uint8_t flagAndOffset = reader.readBE<uint8_t>();
      uint8_t flag = (flagAndOffset >> 5) & 0b00000111;

      const std::tuple<uint16_t, const char *, miniid> flagTable[] = {
          std::make_tuple(0x1, "Reserved", MID("reserved")),
          std::make_tuple(0x2, "Don\'t Fragment", MID("dontFrag")),
          std::make_tuple(0x4, "More Fragments", MID("moreFrag")),
      };

      Property *flags = new Property(MID("flags"), flag);
      std::string flagSummary;
      for (const auto &bit : flagTable) {
        bool on = std::get<0>(bit) & flag;
        Property *flagBit = new Property(std::get<2>(bit), on);
        flagBit->setRange(reader.lastRange());
        flagBit->setError(reader.lastError());
        flags->addProperty(flagBit);
        if (on) {
          if (!flagSummary.empty())
            flagSummary += ", ";
          flagSummary += std::get<1>(bit);
        }
      }
      flags->setSummary(flagSummary);
      flags->setRange(reader.lastRange());
      flags->setError(reader.lastError());
      child->addProperty(flags);

      uint16_t fgOffset =
          ((flagAndOffset & 0b00011111) << 8) | reader.readBE<uint8_t>();
      Property *fragmentOffset = new Property(MID("fOffset"), fgOffset);
      fragmentOffset->setRange(std::make_pair(6, 8));
      fragmentOffset->setError(reader.lastError());
      child->addProperty(fragmentOffset);

      Property *ttl = new Property(MID("ttl"), reader.readBE<uint8_t>());
      ttl->setRange(reader.lastRange());
      ttl->setError(reader.lastError());
      child->addProperty(ttl);

      const std::unordered_map<uint16_t, std::pair<std::string, miniid>>
          protoTable = {
              {0x01, std::make_pair("ICMP", MNS("*icmp"))},
              {0x02, std::make_pair("IGMP", MNS("*igmp"))},
              {0x06, std::make_pair("TCP", MNS("*tcp"))},
              {0x11, std::make_pair("UDP", MNS("*udp"))},
          };

      uint8_t protocolNumber = reader.readBE<uint8_t>();
      Property *proto = new Property(MID("protocol"), protocolNumber);
      const auto &type = fmt::enums(protoTable, protocolNumber,
                                    std::make_pair("Unknown", MNS("?")));
      proto->setSummary(type.first);
      if (type.second != MNS("?")) {
        child->setNs(minins(MNS("ipv4"), type.second));
      }
      proto->setRange(reader.lastRange());
      proto->setError(reader.lastError());
      child->addProperty(proto);

      Property *checksum =
          new Property(MID("checksum"), reader.readBE<uint16_t>());
      checksum->setRange(reader.lastRange());
      checksum->setError(reader.lastError());
      child->addProperty(checksum);

      const auto &srcSlice = reader.slice(4);
      Property *src = new Property(MID("src"), srcSlice);
      src->setSummary(fmt::toDec(srcSlice, 1));
      src->setRange(reader.lastRange());
      src->setError(reader.lastError());
      child->addProperty(src);

      const auto &dstSlice = reader.slice(4);
      Property *dst = new Property(MID("dst"), dstSlice);
      dst->setSummary(fmt::toDec(dstSlice, 1));
      dst->setRange(reader.lastRange());
      dst->setError(reader.lastError());
      child->addProperty(dst);

      child->setSummary("[" + proto->summary() + "] " + src->summary() +
                        " -> " + dst->summary());

      child->setPayload(reader.slice(totalLength - 20));
      return child;
    }
  };

public:
  Dissector::WorkerPtr createWorker() override {
    return Dissector::WorkerPtr(new IPv4Dissector::Worker());
  }
  std::vector<minins> namespaces() const override {
    return std::vector<minins>{MNS("*ipv4")};
  }
};

class IPv4DissectorFactory final : public DissectorFactory {
public:
  DissectorPtr create(const SessionContext &ctx) const override {
    return DissectorPtr(new IPv4Dissector());
  }
};

void Init(v8::Local<v8::Object> exports) {
  exports->Set(
      Nan::New("factory").ToLocalChecked(),
      DissectorFactory::wrap(std::make_shared<IPv4DissectorFactory>()));
}

NODE_MODULE(dissectorEssentials, Init);