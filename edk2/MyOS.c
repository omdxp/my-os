/** @file
  MyOS UEFI bootloader main source file.

  Loads kernel.bin from the current filesystem, copies it to 0x100000,
  exits UEFI boot services, and jumps to the kernel.

  Copyright (c) 2025, omdxp. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Guid/FileInfo.h>
#include "kernel/src/config.h"
#include <Library/BaseMemoryLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

typedef struct __attribute__((packed)) E820Entry
{
  UINT64 base_addr;
  UINT64 length;
  UINT32 type;
  UINT32 extended_attributes;
} E820Entry;

typedef struct __attribute__((packed)) E820Entries
{
  UINT64 count;
  E820Entry entries[];
} E820Entries;

EFI_HANDLE imageHandle = NULL;
EFI_SYSTEM_TABLE *systemTable = NULL;

EFI_STATUS SetupMemoryMaps()
{
  EFI_STATUS Status;
  UINTN MemoryMapSize = 0;
  EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
  UINTN MapKey;
  UINTN DescriptorSize;
  UINT32 DescriptorVersion;

  Status = gBS->GetMemoryMap(
      &MemoryMapSize,
      MemoryMap,
      &MapKey,
      &DescriptorSize,
      &DescriptorVersion);

  if (Status != EFI_BUFFER_TOO_SMALL && EFI_ERROR(Status))
  {
    Print(L"GetMemoryMap error: %r\n", Status);
    return Status;
  }

  // Allocate some extra space for new memory map entries
  MemoryMapSize += DescriptorSize * 10;
  MemoryMap = AllocatePool(MemoryMapSize);
  if (MemoryMap == NULL)
  {
    Print(L"AllocatePool MemoryMap error\n");
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gBS->GetMemoryMap(
      &MemoryMapSize,
      MemoryMap,
      &MapKey,
      &DescriptorSize,
      &DescriptorVersion);

  if (EFI_ERROR(Status))
  {
    Print(L"GetMemoryMap error: %r\n", Status);
    FreePool(MemoryMap);
    return Status;
  }

  UINTN DescriptorCount = MemoryMapSize / DescriptorSize;
  EFI_MEMORY_DESCRIPTOR *Descriptor = MemoryMap;
  UINTN TotalConventionalDescriptors = 0;
  for (UINTN Index = 0; Index < DescriptorCount; Index++)
  {
    if (Descriptor->Type == EfiConventionalMemory)
    {
      TotalConventionalDescriptors++;
    }
    Descriptor = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Descriptor + DescriptorSize);
  }

  EFI_PHYSICAL_ADDRESS MemoryMapLocationE820 = MYOS_MEMORY_MAP_TOTAL_ENTRIES_LOCATION;
  UINTN MemoryMapSizeE820 = sizeof(UINT64) + TotalConventionalDescriptors * sizeof(E820Entry);
  Status = gBS->AllocatePages(
      AllocateAddress,
      EfiLoaderData,
      EFI_SIZE_TO_PAGES(MemoryMapSizeE820),
      &MemoryMapLocationE820);

  if (EFI_ERROR(Status))
  {
    Print(L"AllocatePages MemoryMapLocationE820 error: %r\n", Status);
    FreePool(MemoryMap);
    return Status;
  }

  E820Entries *E820 = (E820Entries *)MemoryMapLocationE820;
  UINTN ConventionalMemoryIndex = 0;
  Descriptor = MemoryMap;
  for (UINTN Index = 0; Index < DescriptorCount; Index++)
  {
    if (Descriptor->Type == EfiConventionalMemory)
    {
      E820Entry *Entry = &E820->entries[ConventionalMemoryIndex++];
      Entry->base_addr = Descriptor->PhysicalStart;
      Entry->length = Descriptor->NumberOfPages * 4096;
      Entry->type = 1; // Usable RAM
      Entry->extended_attributes = 0;
      Print(L"E820 Entry %u: BaseAddr: 0x%lx, Length: 0x%lx, Type: %u\n",
            ConventionalMemoryIndex - 1,
            Entry->base_addr,
            Entry->length,
            Entry->type);
      ConventionalMemoryIndex++;
    }
    Descriptor = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Descriptor + DescriptorSize);
  }

  E820->count = TotalConventionalDescriptors;
  Print(L"E820 Total Entries: %lu\n", E820->count);
  FreePool(MemoryMap);
  return EFI_SUCCESS;
}

EFI_STATUS ReadFileFromCurrentFilesystem(CHAR16 *FileName, VOID **Buffer_Out, UINTN *BufferSize_Out)
{
  EFI_STATUS Status = 0;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImageProtocol = NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystemProtocol = NULL;

  EFI_FILE_PROTOCOL *Root = NULL;
  EFI_FILE_PROTOCOL *File = NULL;
  UINTN FileInfoSize = 0;

  *Buffer_Out = NULL;
  *BufferSize_Out = 0;

  Status = gBS->HandleProtocol(
      imageHandle,
      &gEfiLoadedImageProtocolGuid,
      (VOID **)&LoadedImageProtocol);

  if (EFI_ERROR(Status))
  {
    Print(L"HandleProtocol LoadedImageProtocol error: %r\n", Status);
    return Status;
  }

  Status = gBS->HandleProtocol(
      LoadedImageProtocol->DeviceHandle,
      &gEfiSimpleFileSystemProtocolGuid,
      (VOID **)&SimpleFileSystemProtocol);

  if (EFI_ERROR(Status))
  {
    Print(L"HandleProtocol SimpleFileSystemProtocol error: %r\n", Status);
    return Status;
  }

  Status = SimpleFileSystemProtocol->OpenVolume(
      SimpleFileSystemProtocol,
      &Root);

  if (EFI_ERROR(Status))
  {
    Print(L"OpenVolume error: %r\n", Status);
    return Status;
  }

  Status = Root->Open(
      Root,
      &File,
      FileName,
      EFI_FILE_MODE_READ,
      0);

  if (EFI_ERROR(Status))
  {
    Print(L"Open file error: %r\n", Status);
    return Status;
  }

  FileInfoSize = OFFSET_OF(EFI_FILE_INFO, FileName) + 256 * sizeof(CHAR16);
  VOID *FileInfoBuffer = AllocatePool(FileInfoSize);
  if (FileInfoBuffer == NULL)
  {
    Print(L"AllocatePool FileInfoBuffer error\n");
    File->Close(File);
    return EFI_OUT_OF_RESOURCES;
  }

  EFI_FILE_INFO *FileInfo = (EFI_FILE_INFO *)FileInfoBuffer;
  Status = File->GetInfo(
      File,
      &gEfiFileInfoGuid,
      &FileInfoSize,
      FileInfo);

  if (EFI_ERROR(Status))
  {
    Print(L"GetInfo error: %r\n", Status);
    FreePool(FileInfoBuffer);
    File->Close(File);
    return Status;
  }

  UINTN BufferSize = FileInfo->FileSize;
  FreePool(FileInfoBuffer);
  FileInfoBuffer = NULL;

  VOID *Buffer = AllocatePool(BufferSize);
  if (Buffer == NULL)
  {
    Print(L"AllocatePool Buffer error\n");
    File->Close(File);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = File->Read(
      File,
      &BufferSize,
      Buffer);

  if (EFI_ERROR(Status))
  {
    Print(L"Read file error: %r\n", Status);
    FreePool(Buffer);
    File->Close(File);
    return Status;
  }

  File->Close(File);
  *Buffer_Out = Buffer;
  *BufferSize_Out = BufferSize;
  return EFI_SUCCESS;
}
/**
  The user Entry Point for Application. The user code starts with this function
  as the real entry point for the application.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
UefiMain(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable)
{
  imageHandle = ImageHandle;
  systemTable = SystemTable;

  Print(L"MyOS 64 Bit booting!\n");

  VOID *KernelBuffer = NULL;
  UINTN KernelBufferSize = 0;
  EFI_STATUS Status = ReadFileFromCurrentFilesystem(L"kernel.bin", &KernelBuffer, &KernelBufferSize);
  if (EFI_ERROR(Status))
  {
    Print(L"ReadFileFromCurrentFilesystem error: %r\n", Status);
    return Status;
  }

  Print(L"KernelBuffer: %p, KernelBufferSize: %u\n", KernelBuffer, KernelBufferSize);

  // kernel must be mapped at 0x100000
  EFI_PHYSICAL_ADDRESS KernelBase = MYOS_KERNEL_LOCATION;
  Status = gBS->AllocatePages(
      AllocateAddress,
      EfiLoaderData,
      EFI_SIZE_TO_PAGES(KernelBufferSize),
      &KernelBase);
  if (EFI_ERROR(Status))
  {
    Print(L"AllocatePages error: %r\n", Status);
    FreePool(KernelBuffer);
    return Status;
  }

  // copy kernel to 0x100000
  CopyMem((VOID *)KernelBase, KernelBuffer, KernelBufferSize);
  Print(L"Kernel copied to %p\n", (VOID *)KernelBase);

  // setup memory maps
  Status = SetupMemoryMaps();
  if (EFI_ERROR(Status))
  {
    Print(L"SetupMemoryMaps error: %r\n", Status);
    FreePool(KernelBuffer);
    return Status;
  }

  // end uefi services
  gBS->ExitBootServices(imageHandle, 0);

  // jump to kernel
  __asm__("jmp *%0"
          :
          : "r"(KernelBase));

  return EFI_SUCCESS;
}
