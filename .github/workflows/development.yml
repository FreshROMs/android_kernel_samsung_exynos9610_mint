name: Development Build (Galaxy A50)

on:
  push:
    branches:
      - 'master'
      - 'main'
      - 'wip-*'
  workflow_dispatch:
    inputs:
      version:
        description: 'Build KSU'
        required: false    

jobs:
  oneui-s-e:
    name: Build One UI 4 (Enforcing)
    runs-on: ubuntu-latest

    strategy:
      fail-fast: true
      
    steps:
    - uses: actions/checkout@v2

    - uses: szenius/set-timezone@v1.0
      with:
        timezoneLinux: "Asia/Manila"
        timezoneMacos: "Asia/Manila"
        timezoneWindows: "Philippine Standard Time"

    - name: Export build branch
      run: echo "##[set-output name=branch;]$(echo ${GITHUB_REF#refs/heads/})"
      id: branch_name

    - name: Update Debian/Ubuntu Repositories
      run: sudo apt-get update

    - name: Install Debian/Ubuntu dependencies
      run: sudo apt-get install bzip2 lib32stdc++6 libc6-dev-i386 libncurses5 jq -y

    - name: Build Mint kernel
      run: |
          set -eo pipefail
          echo "  I: Building Mint kernel ${GITHUB_REF##*/}-${GITHUB_RUN_NUMBER}"
          ./build.sh --magisk --automated --device a50 --variant oneui --android 12

    - name: Prepare release package
      run: |
          mkdir -p ./release
          mv -f `find ./ -iname Mint*-*.zip` ./release/

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: One UI Four Kernel ZIPs
        path: 'release'
        if-no-files-found: error

    - name: Prepare build config artifact
      run: |
          cp .config ./release/kernel_config_a50_oneui-s-e.txt

    - name: Upload kernel image artifact
      uses: actions/upload-artifact@v2
      with:
        name: One UI 4 Kernel Image (Enforcing)
        path: 'tools/make/boot.img'
        if-no-files-found: error

    - name: Upload build config artifact
      uses: actions/upload-artifact@v2
      with:
        name: Kernel Configs
        path: 'release/kernel_config_a50_oneui-s-e.txt'
        if-no-files-found: error

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: Release
        path: 'release'
        if-no-files-found: error

  oneui-s-m:
    name: Build One UI 4 (Enforcing; Magisk Canary)
    runs-on: ubuntu-latest

    strategy:
      fail-fast: true
      
    steps:
    - uses: actions/checkout@v2

    - uses: szenius/set-timezone@v1.0
      with:
        timezoneLinux: "Asia/Manila"
        timezoneMacos: "Asia/Manila"
        timezoneWindows: "Philippine Standard Time"

    - name: Export build branch
      run: echo "##[set-output name=branch;]$(echo ${GITHUB_REF#refs/heads/})"
      id: branch_name

    - name: Update Debian/Ubuntu Repositories
      run: sudo apt-get update

    - name: Install Debian/Ubuntu dependencies
      run: sudo apt-get install bzip2 lib32stdc++6 libc6-dev-i386 libncurses5 jq -y

    - name: Build Mint kernel
      run: |
          set -eo pipefail
          echo "  I: Building Mint kernel ${GITHUB_REF##*/}-${GITHUB_RUN_NUMBER}"
          ./build.sh --magisk canary --automated --device a50 --variant oneui --android 12

    - name: Prepare release package
      run: |
          mkdir -p ./release
          mv -f `find ./ -iname Mint*-*.zip` ./release/

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: One UI Four Kernel ZIPs (Canary)
        path: 'release'
        if-no-files-found: error

    - name: Prepare build config artifact
      run: |
          cp .config ./release/kernel_config_a50_oneui-s-c.txt

    - name: Upload kernel image artifact
      uses: actions/upload-artifact@v2
      with:
        name: One UI 4 Kernel Image (Enforcing; Canary)
        path: 'tools/make/boot.img'
        if-no-files-found: error

    - name: Upload build config artifact
      uses: actions/upload-artifact@v2
      with:
        name: Kernel Configs
        path: 'release/kernel_config_a50_oneui-s-c.txt'
        if-no-files-found: error

  oneui-s-k-e:
    name: Build One UI 4 (Enforcing ; KernelSU)
    runs-on: ubuntu-latest

    strategy:
      fail-fast: true
      
    steps:
    - uses: actions/checkout@v2

    - uses: szenius/set-timezone@v1.0
      with:
        timezoneLinux: "Asia/Manila"
        timezoneMacos: "Asia/Manila"
        timezoneWindows: "Philippine Standard Time"

    - name: Export build branch
      run: echo "##[set-output name=branch;]$(echo ${GITHUB_REF#refs/heads/})"
      id: branch_name

    - name: Update Debian/Ubuntu Repositories
      run: sudo apt-get update

    - name: Install Debian/Ubuntu dependencies
      run: sudo apt-get install bzip2 lib32stdc++6 libc6-dev-i386 libncurses5 jq -y

    - name: Build Mint kernel
      run: |
          set -eo pipefail
          echo "  I: Building Mint kernel ${GITHUB_REF##*/}-${GITHUB_RUN_NUMBER}"
          ./build.sh --kernelsu --automated --device a50 --variant oneui --android 12

    - name: Prepare release package
      run: |
          mkdir -p ./release
          mv -f `find ./ -iname Mint*-*.zip` ./release/

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: One UI Four Kernel ZIPs
        path: 'release'
        if-no-files-found: error

    - name: Prepare build config artifact
      run: |
          cp .config ./release/kernel_config_a50_oneui-s-k-e.txt

    - name: Upload kernel image artifact
      uses: actions/upload-artifact@v2
      with:
        name: One UI 4 Kernel Image (Enforcing)
        path: 'tools/make/boot.img'
        if-no-files-found: error

    - name: Upload build config artifact
      uses: actions/upload-artifact@v2
      with:
        name: Kernel Configs
        path: 'release/kernel_config_a50_oneui-s-k-e.txt'
        if-no-files-found: error

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: Release
        path: 'release'
        if-no-files-found: error
  
  oneui-s-p:
    name: Build One UI 4 (Permissive)
    runs-on: ubuntu-latest

    strategy:
      fail-fast: true
      
    steps:
    - uses: actions/checkout@v2

    - uses: szenius/set-timezone@v1.0
      with:
        timezoneLinux: "Asia/Manila"
        timezoneMacos: "Asia/Manila"
        timezoneWindows: "Philippine Standard Time"

    - name: Export build branch
      run: echo "##[set-output name=branch;]$(echo ${GITHUB_REF#refs/heads/})"
      id: branch_name

    - name: Update Debian/Ubuntu Repositories
      run: sudo apt-get update

    - name: Install Debian/Ubuntu dependencies
      run: sudo apt-get install bzip2 lib32stdc++6 libc6-dev-i386 libncurses5 jq -y

    - name: Build Mint kernel
      run: |
          set -eo pipefail
          echo "  I: Building Mint kernel ${GITHUB_REF##*/}-${GITHUB_RUN_NUMBER}"
          ./build.sh --magisk --automated --device a50 --variant oneui --android 12 --permissive

    - name: Prepare release package
      run: |
          mkdir -p ./release
          mv -f `find ./ -iname Mint*-*.zip` ./release/

    - name: Prepare build config artifact
      run: |
          cp .config ./release/kernel_config_a50_oneui-s-p.txt

    - name: Upload kernel image artifact
      uses: actions/upload-artifact@v2
      with:
        name: One UI 4 Kernel Image (Permissive)
        path: 'tools/make/boot.img'
        if-no-files-found: error

    - name: Upload build config artifact
      uses: actions/upload-artifact@v2
      with:
        name: Kernel Configs
        path: 'release/kernel_config_a50_oneui-s-p.txt'
        if-no-files-found: error

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: Release
        path: 'release'
        if-no-files-found: error

  oneui-s-k-p:
    name: Build One UI 4 (Permissive ; KernelSU)
    runs-on: ubuntu-latest

    strategy:
      fail-fast: true
      
    steps:
    - uses: actions/checkout@v2

    - uses: szenius/set-timezone@v1.0
      with:
        timezoneLinux: "Asia/Manila"
        timezoneMacos: "Asia/Manila"
        timezoneWindows: "Philippine Standard Time"

    - name: Export build branch
      run: echo "##[set-output name=branch;]$(echo ${GITHUB_REF#refs/heads/})"
      id: branch_name

    - name: Update Debian/Ubuntu Repositories
      run: sudo apt-get update

    - name: Install Debian/Ubuntu dependencies
      run: sudo apt-get install bzip2 lib32stdc++6 libc6-dev-i386 libncurses5 jq -y

    - name: Build Mint kernel
      run: |
          set -eo pipefail
          echo "  I: Building Mint kernel ${GITHUB_REF##*/}-${GITHUB_RUN_NUMBER}"
          ./build.sh --kernelsu --automated --device a50 --variant oneui --android 12 --permissive

    - name: Prepare release package
      run: |
          mkdir -p ./release
          mv -f `find ./ -iname Mint*-*.zip` ./release/

    - name: Prepare build config artifact
      run: |
          cp .config ./release/kernel_config_a50_oneui-s-k-p.txt

    - name: Upload kernel image artifact
      uses: actions/upload-artifact@v2
      with:
        name: One UI 4 Kernel Image (Permissive)
        path: 'tools/make/boot.img'
        if-no-files-found: error

    - name: Upload build config artifact
      uses: actions/upload-artifact@v2
      with:
        name: Kernel Configs
        path: 'release/kernel_config_a50_oneui-s-k-p.txt'
        if-no-files-found: error

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: Release
        path: 'release'
        if-no-files-found: error
  
  aosp-s-e:
    name: Build AOSP 12 (Enforcing)
    if: ${{ !contains(github.event.head_commit.message, '[skip rel]') }}
    runs-on: ubuntu-latest

    strategy:
      fail-fast: true
      
    steps:
    - uses: actions/checkout@v2

    - uses: szenius/set-timezone@v1.0
      with:
        timezoneLinux: "Asia/Manila"
        timezoneMacos: "Asia/Manila"
        timezoneWindows: "Philippine Standard Time"

    - name: Export build branch
      run: echo "##[set-output name=branch;]$(echo ${GITHUB_REF#refs/heads/})"
      id: branch_name

    - name: Update Debian/Ubuntu Repositories
      run: sudo apt-get update

    - name: Install Debian/Ubuntu dependencies
      run: sudo apt-get install bzip2 lib32stdc++6 libc6-dev-i386 libncurses5 jq -y

    - name: Build Mint kernel
      run: |
          set -eo pipefail
          echo "  I: Building Mint kernel ${GITHUB_REF##*/}-${GITHUB_RUN_NUMBER}"
          ./build.sh --magisk --automated --device a50 --variant aosp --android 12

    - name: Prepare release package
      run: |
          mkdir -p ./release
          mv -f `find ./ -iname Mint*-*.zip` ./release/

    - name: Prepare build config artifact
      run: |
          cp .config ./release/kernel_config_a50_aosp-s-e.txt

    - name: Upload kernel image artifact
      uses: actions/upload-artifact@v2
      with:
        name: AOSP 12 Kernel Image (Enforcing)
        path: 'tools/make/boot.img'
        if-no-files-found: error

    - name: Upload build config artifact
      uses: actions/upload-artifact@v2
      with:
        name: Kernel Configs
        path: 'release/kernel_config_a50_aosp-s-e.txt'
        if-no-files-found: error

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: Release
        path: 'release'
        if-no-files-found: error

  aosp-s-k-e:
    name: Build AOSP 12 (Enforcing ; KernelSU)
    if: ${{ !contains(github.event.head_commit.message, '[skip rel]') }}
    runs-on: ubuntu-latest

    strategy:
      fail-fast: true
      
    steps:
    - uses: actions/checkout@v2

    - uses: szenius/set-timezone@v1.0
      with:
        timezoneLinux: "Asia/Manila"
        timezoneMacos: "Asia/Manila"
        timezoneWindows: "Philippine Standard Time"

    - name: Export build branch
      run: echo "##[set-output name=branch;]$(echo ${GITHUB_REF#refs/heads/})"
      id: branch_name

    - name: Update Debian/Ubuntu Repositories
      run: sudo apt-get update

    - name: Install Debian/Ubuntu dependencies
      run: sudo apt-get install bzip2 lib32stdc++6 libc6-dev-i386 libncurses5 jq -y

    - name: Build Mint kernel
      run: |
          set -eo pipefail
          echo "  I: Building Mint kernel ${GITHUB_REF##*/}-${GITHUB_RUN_NUMBER}"
          ./build.sh --kernelsu --automated --device a50 --variant aosp --android 12

    - name: Prepare release package
      run: |
          mkdir -p ./release
          mv -f `find ./ -iname Mint*-*.zip` ./release/

    - name: Prepare build config artifact
      run: |
          cp .config ./release/kernel_config_a50_aosp-s-k-e.txt

    - name: Upload kernel image artifact
      uses: actions/upload-artifact@v2
      with:
        name: AOSP 12 Kernel Image (Enforcing)
        path: 'tools/make/boot.img'
        if-no-files-found: error

    - name: Upload build config artifact
      uses: actions/upload-artifact@v2
      with:
        name: Kernel Configs
        path: 'release/kernel_config_a50_aosp-s-k-e.txt'
        if-no-files-found: error

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: Release
        path: 'release'
        if-no-files-found: error
  
  aosp-s-p:
    name: Build AOSP 12 (Permissive)
    if: ${{ !contains(github.event.head_commit.message, '[skip rel]') }}
    runs-on: ubuntu-latest

    strategy:
      fail-fast: true
      
    steps:
    - uses: actions/checkout@v2

    - uses: szenius/set-timezone@v1.0
      with:
        timezoneLinux: "Asia/Manila"
        timezoneMacos: "Asia/Manila"
        timezoneWindows: "Philippine Standard Time"

    - name: Export build branch
      run: echo "##[set-output name=branch;]$(echo ${GITHUB_REF#refs/heads/})"
      id: branch_name

    - name: Update Debian/Ubuntu Repositories
      run: sudo apt-get update

    - name: Install Debian/Ubuntu dependencies
      run: sudo apt-get install bzip2 lib32stdc++6 libc6-dev-i386 libncurses5 jq -y

    - name: Build Mint kernel
      run: |
          set -eo pipefail
          echo "  I: Building Mint kernel ${GITHUB_REF##*/}-${GITHUB_RUN_NUMBER}"
          ./build.sh --magisk --automated --device a50 --variant aosp --android 12 --permissive

    - name: Prepare release package
      run: |
          mkdir -p ./release
          mv -f `find ./ -iname Mint*-*.zip` ./release/

    - name: Prepare build config artifact
      run: |
          cp .config ./release/kernel_config_a50_aosp-s-p.txt

    - name: Upload kernel image artifact
      uses: actions/upload-artifact@v2
      with:
        name: AOSP 12 Kernel Image (Permissive)
        path: 'tools/make/boot.img'
        if-no-files-found: error

    - name: Upload build config artifact
      uses: actions/upload-artifact@v2
      with:
        name: Kernel Configs
        path: 'release/kernel_config_a50_aosp-s-p.txt'
        if-no-files-found: error

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: Release
        path: 'release'
        if-no-files-found: error
        
  aosp-s-k-p:
    name: Build AOSP 12 (Permissive ; KernelSU)
    if: ${{ !contains(github.event.head_commit.message, '[skip rel]') }}
    runs-on: ubuntu-latest

    strategy:
      fail-fast: true
      
    steps:
    - uses: actions/checkout@v2

    - uses: szenius/set-timezone@v1.0
      with:
        timezoneLinux: "Asia/Manila"
        timezoneMacos: "Asia/Manila"
        timezoneWindows: "Philippine Standard Time"

    - name: Export build branch
      run: echo "##[set-output name=branch;]$(echo ${GITHUB_REF#refs/heads/})"
      id: branch_name

    - name: Update Debian/Ubuntu Repositories
      run: sudo apt-get update

    - name: Install Debian/Ubuntu dependencies
      run: sudo apt-get install bzip2 lib32stdc++6 libc6-dev-i386 libncurses5 jq -y

    - name: Build Mint kernel
      run: |
          set -eo pipefail
          echo "  I: Building Mint kernel ${GITHUB_REF##*/}-${GITHUB_RUN_NUMBER}"
          ./build.sh --kernelsu --automated --device a50 --variant aosp --android 12 --permissive

    - name: Prepare release package
      run: |
          mkdir -p ./release
          mv -f `find ./ -iname Mint*-*.zip` ./release/

    - name: Prepare build config artifact
      run: |
          cp .config ./release/kernel_config_a50_aosp-s-k-p.txt

    - name: Upload kernel image artifact
      uses: actions/upload-artifact@v2
      with:
        name: AOSP 12 Kernel Image (Permissive)
        path: 'tools/make/boot.img'
        if-no-files-found: error

    - name: Upload build config artifact
      uses: actions/upload-artifact@v2
      with:
        name: Kernel Configs
        path: 'release/kernel_config_a50_aosp-s-k-p.txt'
        if-no-files-found: error

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: Release
        path: 'release'
        if-no-files-found: error

  oneui-r-e:
    name: Build One UI 3 (Enforcing)
    if: ${{ !contains(github.event.head_commit.message, '[skip rel]') }}
    runs-on: ubuntu-latest

    strategy:
      fail-fast: true
      
    steps:
    - uses: actions/checkout@v2

    - uses: szenius/set-timezone@v1.0
      with:
        timezoneLinux: "Asia/Manila"
        timezoneMacos: "Asia/Manila"
        timezoneWindows: "Philippine Standard Time"

    - name: Export build branch
      run: echo "##[set-output name=branch;]$(echo ${GITHUB_REF#refs/heads/})"
      id: branch_name

    - name: Update Debian/Ubuntu Repositories
      run: sudo apt-get update

    - name: Install Debian/Ubuntu dependencies
      run: sudo apt-get install bzip2 lib32stdc++6 libc6-dev-i386 libncurses5 jq -y

    - name: Build Mint kernel
      run: |
          set -eo pipefail
          echo "  I: Building Mint kernel ${GITHUB_REF##*/}-${GITHUB_RUN_NUMBER}"
          ./build.sh --magisk --automated --device a50 --variant oneui --android 11

    - name: Prepare release package
      run: |
          mkdir -p ./release
          mv -f `find ./ -iname Mint*-*.zip` ./release/

    - name: Prepare build config artifact
      run: |
          cp .config ./release/kernel_config_a50_oneui-r-e.txt

    - name: Upload kernel image artifact
      uses: actions/upload-artifact@v2
      with:
        name: One UI 3 Kernel Image (Enforcing)
        path: 'tools/make/boot.img'
        if-no-files-found: error

    - name: Upload build config artifact
      uses: actions/upload-artifact@v2
      with:
        name: Kernel Configs
        path: 'release/kernel_config_a50_oneui-r-e.txt'
        if-no-files-found: error

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: Release
        path: 'release'
        if-no-files-found: error

  oneui-r-p:
    name: Build One UI 3 (Permissive)
    if: ${{ !contains(github.event.head_commit.message, '[skip rel]') }}
    runs-on: ubuntu-latest

    strategy:
      fail-fast: true
      
    steps:
    - uses: actions/checkout@v2

    - uses: szenius/set-timezone@v1.0
      with:
        timezoneLinux: "Asia/Manila"
        timezoneMacos: "Asia/Manila"
        timezoneWindows: "Philippine Standard Time"

    - name: Export build branch
      run: echo "##[set-output name=branch;]$(echo ${GITHUB_REF#refs/heads/})"
      id: branch_name

    - name: Update Debian/Ubuntu Repositories
      run: sudo apt-get update

    - name: Install Debian/Ubuntu dependencies
      run: sudo apt-get install bzip2 lib32stdc++6 libc6-dev-i386 libncurses5 jq -y

    - name: Build Mint kernel
      run: |
          set -eo pipefail
          echo "  I: Building Mint kernel ${GITHUB_REF##*/}-${GITHUB_RUN_NUMBER}"
          ./build.sh --magisk --automated --device a50 --variant oneui --android 11 --permissive

    - name: Prepare release package
      run: |
          mkdir -p ./release
          mv -f `find ./ -iname Mint*-*.zip` ./release/

    - name: Prepare build config artifact
      run: |
          cp .config ./release/kernel_config_a50_oneui-r-p.txt

    - name: Upload kernel image artifact
      uses: actions/upload-artifact@v2
      with:
        name: One UI 3 Kernel Image (Permissive)
        path: 'tools/make/boot.img'
        if-no-files-found: error

    - name: Upload build config artifact
      uses: actions/upload-artifact@v2
      with:
        name: Kernel Configs
        path: 'release/kernel_config_a50_oneui-r-p.txt'
        if-no-files-found: error

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: Release
        path: 'release'
        if-no-files-found: error

  aosp-r-e:
    name: Build AOSP 11 (Enforcing)
    if: ${{ !contains(github.event.head_commit.message, '[skip rel]') }}
    runs-on: ubuntu-latest

    strategy:
      fail-fast: true
      
    steps:
    - uses: actions/checkout@v2

    - uses: szenius/set-timezone@v1.0
      with:
        timezoneLinux: "Asia/Manila"
        timezoneMacos: "Asia/Manila"
        timezoneWindows: "Philippine Standard Time"

    - name: Export build branch
      run: echo "##[set-output name=branch;]$(echo ${GITHUB_REF#refs/heads/})"
      id: branch_name

    - name: Update Debian/Ubuntu Repositories
      run: sudo apt-get update

    - name: Install Debian/Ubuntu dependencies
      run: sudo apt-get install bzip2 lib32stdc++6 libc6-dev-i386 libncurses5 jq -y

    - name: Build Mint kernel
      run: |
          set -eo pipefail
          echo "  I: Building Mint kernel ${GITHUB_REF##*/}-${GITHUB_RUN_NUMBER}"
          ./build.sh --magisk --automated --device a50 --variant aosp --android 11

    - name: Prepare release package
      run: |
          mkdir -p ./release
          mv -f `find ./ -iname Mint*-*.zip` ./release/

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: One UI Four Kernel ZIPs
        path: 'release'
        if-no-files-found: error

    - name: Prepare build config artifact
      run: |
          cp .config ./release/kernel_config_a50_aosp-r-e.txt

    - name: Upload kernel image artifact
      uses: actions/upload-artifact@v2
      with:
        name: AOSP 11 Kernel Image (Enforcing)
        path: 'tools/make/boot.img'
        if-no-files-found: error

    - name: Upload build config artifact
      uses: actions/upload-artifact@v2
      with:
        name: Kernel Configs
        path: 'release/kernel_config_a50_aosp-r-e.txt'
        if-no-files-found: error

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: Release
        path: 'release'
        if-no-files-found: error

  aosp-r-p:
    name: Build AOSP 11 (Permissive)
    if: ${{ !contains(github.event.head_commit.message, '[skip rel]') }}
    runs-on: ubuntu-latest

    strategy:
      fail-fast: true
      
    steps:
    - uses: actions/checkout@v2

    - uses: szenius/set-timezone@v1.0
      with:
        timezoneLinux: "Asia/Manila"
        timezoneMacos: "Asia/Manila"
        timezoneWindows: "Philippine Standard Time"

    - name: Export build branch
      run: echo "##[set-output name=branch;]$(echo ${GITHUB_REF#refs/heads/})"
      id: branch_name

    - name: Update Debian/Ubuntu Repositories
      run: sudo apt-get update

    - name: Install Debian/Ubuntu dependencies
      run: sudo apt-get install bzip2 lib32stdc++6 libc6-dev-i386 libncurses5 jq -y

    - name: Build Mint kernel
      run: |
          set -eo pipefail
          echo "  I: Building Mint kernel ${GITHUB_REF##*/}-${GITHUB_RUN_NUMBER}"
          ./build.sh --magisk --automated --device a50 --variant aosp --android 11 --permissive

    - name: Prepare release package
      run: |
          mkdir -p ./release
          mv -f `find ./ -iname Mint*-*.zip` ./release/

    - name: Prepare build config artifact
      run: |
          cp .config ./release/kernel_config_a50_aosp-r-p.txt

    - name: Upload kernel image artifact
      uses: actions/upload-artifact@v2
      with:
        name: AOSP 11 Kernel Image (Permissive)
        path: 'tools/make/boot.img'
        if-no-files-found: error

    - name: Upload build config artifact
      uses: actions/upload-artifact@v2
      with:
        name: Kernel Configs
        path: 'release/kernel_config_a50_aosp-r-p.txt'
        if-no-files-found: error

    - name: Upload release package
      uses: actions/upload-artifact@v2
      with:
        name: Release
        path: 'release'
        if-no-files-found: error

  release:
    name: Release files and configs
    if: ${{ !contains(github.event.head_commit.message, '[skip rel]') }}
    needs: [oneui-s-e, oneui-s-k-e, oneui-s-p, oneui-s-k-p, oneui-r-e, oneui-r-p, aosp-s-e, aosp-s-k-e, aosp-s-p , aosp-s-k-p, aosp-r-e, aosp-r-p]
    runs-on: ubuntu-latest
    strategy:
      fail-fast: true
    steps:
    - uses: actions/checkout@v2

    - uses: szenius/set-timezone@v1.0
      with:
        timezoneLinux: "Asia/Manila"
        timezoneMacos: "Asia/Manila"
        timezoneWindows: "Philippine Standard Time"

    - name: Merge Releases
      uses: actions/download-artifact@v2
      with:
        name: Release
        path: release

    - name: Delete build config files
      run: |
          set -eo pipefail
          rm -rf ./release/kernel_config_*.txt

    - name: Delete older release
      uses: dev-drprasad/delete-older-releases@v0.2.0
      with:
        keep_latest: 12
        delete_tag_pattern: "xbeta"
      env:
        GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
        
    - name: Upload release
      uses: Hs1r1us/Release-AIO@v1.0
      env:
        GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
      with:
        # The name of the tag
        tag_name: xbeta-${{ github.run_number }}_a50dx
        prerelease: true
        release_name: Galaxy A50 (beta-${{ github.run_number }})
        body_path: "./tools/make/release/a50-development.md"
        asset_files: './release'
