# !/bin/bash

# 安装 homebrew 不允许使用 root 用户
# 编译可能需要 4G 以上的内存，较低配置或可能卡死、或需要多次编译。

# git 换行符推荐设置
# git config --global core.autocrlf input # macOS / Linux
# git config --global core.autocrlf true  # Windows

# 更新包列表
export HTTP_PROXY=192.168.50.100:2080
export HTTPS_PROXY=192.168.50.100:2080
sudo apt update
# 桌面版本需要安装 openssh-server 开启远程连接
# sudo apt install -y openssh-server

# 安装 homebrew 的依赖
sudo apt install -y curl git
alias curl="curl -x 192.168.50.100:2080"
# curl -v -k google.com
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
# homebrew 安装完成后，添加环境变量
echo >> /home/user/.bashrc
echo 'eval "$(/home/linuxbrew/.linuxbrew/bin/brew shellenv bash)"' >> /home/user/.bashrc
eval "$(/home/linuxbrew/.linuxbrew/bin/brew shellenv bash)"

# 安装开发工具
brew install gcc gdb make cmake ninja pkg-config vcpkg sqlcipher
if [ -f /home/linuxbrew/.linuxbrew/bin/gcc-15 ] && [ ! -e /home/linuxbrew/.linuxbrew/bin/gcc ]; then ln -s /home/linuxbrew/.linuxbrew/bin/gcc-15 /home/linuxbrew/.linuxbrew/bin/gcc ; fi
if [ -f /home/linuxbrew/.linuxbrew/bin/g++-15 ] && [ ! -e /home/linuxbrew/.linuxbrew/bin/g++ ]; then ln -s /home/linuxbrew/.linuxbrew/bin/g++-15 /home/linuxbrew/.linuxbrew/bin/g++ ; fi
git clone https://github.com/microsoft/vcpkg.git ~/workspace/tools/vcpkg
export VCPKG_ROOT="$HOME/workspace/tools/vcpkg"
# 验证安装
# which gcc g++ gdb make cmake ninja pkg-config vcpkg sqlcipher
# whereis gcc g++ gdb make cmake ninja pkg-config vcpkg sqlcipher
# echo $PATH
# gcc --version
# g++ --version
# gdb --version
# make --version
# cmake --version
# ninja --version
# pkg-config --version
# vcpkg --version
# sqlcipher --version

# 写入 bashrc 环境变量
echo >> /home/user/.bashrc
echo 'export HTTP_PROXY=192.168.50.100:2080' >> /home/user/.bashrc
echo 'export HTTPS_PROXY=192.168.50.100:2080' >> /home/user/.bashrc
echo 'alias curl="curl -x 192.168.50.100:2080"' >> /home/user/.bashrc
echo 'export VCPKG_ROOT="$HOME/workspace/tools/vcpkg"' >> /home/user/.bashrc

# # 可选优化
# # bash history
# PROMPT_COMMAND='history -a' # 立即写入历史
# HISTTIMEFORMAT="%F %T "     # 记录时间
# HISTSIZE=10000              # 修改记录条数上线
# HISTFILESIZE=10000          # 修改记录条数上线

# 项目配置
mkdir -p ~/workspace/project/qt
# cd ~/workspace/project/qt

# vcpkg 的编译依赖
sudo apt install -y libc6-dev build-essential

# 非编译必须
brew install vim htop
# sudo apt install -y locate
