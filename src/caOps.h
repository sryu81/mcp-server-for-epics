#ifndef INC_caOps_H
#define INC_caOps_H

#include <string>
#include <vector>

namespace caOps {

void init();
void shutdown();

std::string get(const std::vector<std::string> &pvnames, double timeout);
std::string put(const std::string &pvname, const std::string &value, double timeout);
std::string monitor(const std::vector<std::string> &pvnames, double duration);
std::string info(const std::vector<std::string> &pvnames, double timeout);

}

#endif /* INC_caOps_H */
