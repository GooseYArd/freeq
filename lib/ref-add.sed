/^# Packages using this file: / {
  s/# Packages using this file://
  ta
  :a
  s/ freeq / freeq /
  tb
  s/ $/ freeq /
  :b
  s/^/# Packages using this file:/
}
