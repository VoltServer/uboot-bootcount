group "default" {
  targets = ["armhf", "arm64"]
}

target "arm64" {
  context = "."
  dockerfile = "Dockerfile"
  tags = ["bootcount-builder:arm64"]
  args = {
    // this is already the default in the Dockerfile:
    // TARGET_ARCH = "aarch64-linux-gnu"
  }
  output = ["type=local,dest=dist/"]
}

target "armhf" {
  inherits = ["arm64"]
  args = {
    TARGET_ARCH = "arm-linux-gnueabihf"
  }
  tags = ["bootcount-builder:armhf"]
}
