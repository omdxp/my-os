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

EFI_HANDLE imageHandle = NULL;
EFI_SYSTEM_TABLE *systemTable = NULL;

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

  // end uefi services
  gBS->ExitBootServices(imageHandle, 0);

  // jump to kernel
  __asm__("jmp *%0"
          :
          : "r"(KernelBase));

  return EFI_SUCCESS;
}
