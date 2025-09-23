name: Build STM32 HVSP Programmer

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  build:
    runs-on: ubuntu-latest
    
    steps:
    - name: Checkout repository
      uses: actions/checkout@v4
      
    - name: Install ARM toolchain
      run: |
        sudo apt-get update
        sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi
        
    - name: Verify toolchain installation
      run: |
        arm-none-eabi-gcc --version
        arm-none-eabi-as --version
        arm-none-eabi-objcopy --version
        
    - name: Build project
      run: |
        make clean
        make all
        
    - name: Check build artifacts
      run: |
        ls -la build/
        file build/hvsp_programmer.elf
        file build/hvsp_programmer.hex
        file build/hvsp_programmer.bin
        
    - name: Upload build artifacts
      uses: actions/upload-artifact@v4
      with:
        name: hvsp-programmer-firmware
        path: |
          build/hvsp_programmer.elf
          build/hvsp_programmer.hex
          build/hvsp_programmer.bin
        retention-days: 30
        
    - name: Generate release info
      if: github.ref == 'refs/heads/main'
      run: |
        echo "## Build Information" >> release_info.md
        echo "- Commit: ${{ github.sha }}" >> release_info.md
        echo "- Date: $(date)" >> release_info.md
        echo "- Toolchain: $(arm-none-eabi-gcc --version | head -n1)" >> release_info.md
        cat release_info.md
        
    - name: Upload release info
      if: github.ref == 'refs/heads/main'
      uses: actions/upload-artifact@v4
      with:
        name: release-info
        path: release_info.md
