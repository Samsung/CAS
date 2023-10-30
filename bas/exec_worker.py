"""
exec_worker.py - library used for controlling BAS fork server process
"""
import os
import subprocess
import struct

from typing import List, Tuple, Optional


###############################
# Execution worker library
###############################
class ExecWorkerException(Exception):
    pass


class ExecWorker:
    initialized: bool = False

    def __init__(self, workerPath: str = "./worker"):
        self.workerPath = workerPath
        self.initialize()

    def __del__(self):
        self.destroy()


    """ Read precisely `size` bytes from provided file descriptors
        Used to handle partial reads than can occur when using os.read
    """
    def _safe_read(self, fd: int, size: int) -> bytes:
        data = b""
        while size > 0:
            part = os.read(fd, size)
            size -= len(part)
            data += part
        return data

    """ Writes precisely `size` bytes to the provided FD
        Used for the same reason as _safe_read
    """
    def _safe_write(self, fd: int, buf: bytes) -> None:
        offset = 0
        while offset < len(buf):
            part = os.write(fd, buf[offset:])
            offset += part
    

    def initialize(self) -> None:
        self.pRead, self.pChildWrite = os.pipe()
        self.pChildRead, self.pWrite = os.pipe()
        self.worker = subprocess.Popen([self.workerPath, str(self.pChildRead), str(self.pChildWrite)], 
                shell=False, pass_fds=(self.pChildRead, self.pChildWrite))
        
        os.close(self.pChildRead)
        os.close(self.pChildWrite)
        self.initialized = True

        worker_status = self.worker.poll()
        if worker_status is not None:
            self.destroy()
            raise ExecWorkerException(f"Worker process has terminated unexpectedly with error code {worker_status}")


    def destroy(self) -> None:
        if not self.initialized:
            return

        os.close(self.pWrite)
        os.close(self.pRead)
        self.worker.kill()
        self.initialized = False


    def runCmd(self, cwd: str, cmd: str, args: List[str], input: Optional[str] = None) -> Tuple[str, str, int]:
        worker_status = self.worker.poll()
        if worker_status is not None:
            self.destroy()
            raise ExecWorkerException(f"Cannot run command - worker exited with error code {worker_status}")

        if input is None:
            input = ""

        # Send command to worker
        bCwd = cwd.encode()
        bInput = input.encode()
        dataToSend = struct.pack("III", len(bCwd), len(bInput), len(args) + 1)
        dataToSend += bCwd
        dataToSend += bInput
        for arg in [cmd, *args]:
            bArg = arg.encode()
            dataToSend += struct.pack("I", len(bArg))
            dataToSend += bArg
        self._safe_write(self.pWrite, dataToSend)
        
        # Hang on pipe read and retrieve output
        header = self._safe_read(self.pRead, 14)
        sizeOut, sizeErr, retCode, error = struct.unpack("IIiH", header)
        dataOut = self._safe_read(self.pRead, sizeOut)
        dataErr = self._safe_read(self.pRead, sizeErr)

        if error:
            self.destroy()
            raise ExecWorkerException(f"Cannot run command - worker encountered an error {dataErr.decode()}")
        return dataOut.decode(), dataErr.decode(), retCode
