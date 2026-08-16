#pragma once


namespace os {
class FSStreamIO;

class Launchable {
public:
  virtual ~Launchable() = default;

  virtual void launch(FSStreamIO* terminal) = 0;
};

} // namespace os
