## Dependencies

* Bazel: Only tested with an older version of bazel, e.g. (bazel 3.7.0)
* SDL2 (with freetype)
* libgoogle-glog
* libgflags

## Setting up on Ubuntu (18.0.4)

```bash
# Install dependencies
sudo apt-get install curl git
sudo apt-get install libgoogle-glog-dev
sudo apt-get install libgflags-dev
sudo apt-get install libsdl2-dev
sudo apt-get install libsdl2-ttf-dev

```
Install bazel-3.7.0 using the instructions from https://bazel.build/install/ubuntu#install-on-ubuntu

```bash
sudo apt install apt-transport-https curl gnupg -y
curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor >bazel-archive-keyring.gpg
sudo mv bazel-archive-keyring.gpg /usr/share/keyrings
echo "deb [arch=amd64 signed-by=/usr/share/keyrings/bazel-archive-keyring.gpg] https://storage.googleapis.com/bazel-apt stable jdk1.8" | sudo tee /etc/apt/sources.list.d/bazel.list

sudo apt update
sudo apt install bazel-3.7.0

```
## Build
```
cd src
bazel-3.7.0 build -c opt :bman :bman_server
```

## Running

Run single-player mode:

```
./bazel-bin/bman
```

Run with a simple AI agent:

```
./bazel-bin/bman --agent simple
```

Run with a simple AI agent:

```
./bazel-bin/bman --agent simple
```


Running a sever with two agents connected:

```
./bazel-bin/bman_server & ./bazel-bin/bman --agent simple --server 127.0.0.1:8888 & ./bazel-bin/bman  --agent simple --server 127.0.0.1:8888
```