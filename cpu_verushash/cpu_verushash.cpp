// (C) 2018 Michael Toutonghi
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <iostream>
#include <functional>
#include <vector>
#include <stdint.h>
#include <assert.h>

#ifdef WIN32
#include <Windows.h>
#else
#include <string.h>
#include <stdlib.h>
#endif

#include "cpu_verushash.hpp"
#include "primitives/block.h"

void cpu_verushash::start(cpu_verushash& device_context) 
{
	device_context.pVHW = new CVerusHashWriter(SER_GETHASH, PROTOCOL_VERSION);
 	device_context.pVHW2b = new CVerusHashV2bWriter(SER_GETHASH, PROTOCOL_VERSION, device_context.solutionVer);
}

void cpu_verushash::stop(cpu_verushash& device_context) 
{ 
	delete device_context.pVHW;
	delete device_context.pVHW2b;
}

void cpu_verushash::solve_verus(CBlockHeader &bh, 
	arith_uint256 &target,
	std::function<bool()> cancelf,
	std::function<void(const std::vector<uint32_t>&, size_t, const unsigned char*)> solutionf,
	std::function<void(void)> hashdonef,
	cpu_verushash &device_context)
{
	if (bh.nVersion > 4)
	{

		// short circuit to version 2
		if (IsCPUVerusOptimized())
		{
			solve_verus_v2_opt(bh, target, cancelf, solutionf, hashdonef, device_context);
		}
		else
		{
			solve_verus_v2(bh, target, cancelf, solutionf, hashdonef, device_context);
		}
		return;
	}
	CVerusHashWriter &vhw = *(device_context.pVHW);
	CVerusHash &vh = vhw.GetState();
	uint256 curHash;
	std::vector<unsigned char> solution = std::vector<unsigned char>(1344);
	solution[0] = 1; // earliest VerusHash 2.0 solution version
	bh.nSolution = solution;

	// prepare the hash state
	vhw.Reset();
	vhw << bh;

	// clear any extra data
	vh.ClearExtra();

	// loop the requested number of times or until canceled. determine if we 
	// found a winner, and send all winners found as solutions. count only one hash. 
	// hashrate is determined by multiplying hash by VERUSHASHES_PER_SOLVE, with VerusHash, only
	// hashrate and sharerate are valid, solutionrate will equal sharerate
	for (int64_t i = 0; i < VERUSHASHES_PER_SOLVE; i++)
	{
		*(vh.ExtraI64Ptr()) = i;
		vh.ExtraHash((unsigned char *)&curHash);
		if (UintToArith256(curHash) > target)
			continue;

		int extraSpace = (solution.size() % 32) + 15;
		assert(solution.size() > 32);
		*((int64_t *)&(solution.data()[solution.size() - extraSpace])) = i;

		solutionf(std::vector<uint32_t>(0), solution.size(), solution.data());
		if (cancelf()) return;
	}

	hashdonef();
}

void cpu_verushash::solve_verus_v2(CBlockHeader &bh, 
	arith_uint256 &target,
	std::function<bool()> cancelf,
	std::function<void(const std::vector<uint32_t>&, size_t, const unsigned char*)> solutionf,
	std::function<void(void)> hashdonef,
	cpu_verushash &device_context)
{
    if (bh.nSolution.size() && bh.nSolution[0] != device_context.solutionVer)
    {
        device_context.stop(device_context);
        device_context.solutionVer = bh.nSolution[0];
        device_context.start(device_context);
    }

	std::vector<unsigned char> solution = std::vector<unsigned char>();
    if (device_context.solutionVer < 6)
    {
        solution = std::vector<unsigned char>(1344);
        solution[0] = device_context.solutionVer;
		bh.nSolution = solution;
    }
	else
	{
		solution = bh.nSolution = solution;
	}

	CVerusHashV2bWriter &vhw = *(device_context.pVHW2b);
	CVerusHashV2 &vh = vhw.GetState();
    verusclhasher &vclh = vh.vclh;
	uint256 curHash;
    void *hasherrefresh = vclh.gethasherrefresh();
	__m128i **pMoveScratch = vclh.getpmovescratch(hasherrefresh);
    int keyrefreshsize = vclh.keyrefreshsize();

	// prepare the hash state
	vhw.Reset();
	vhw << bh;

	int64_t intermediate, *extraPtr = vhw.xI64p();
	unsigned char *curBuf = vh.CurBuffer();

	// generate a new key for this block
	u128 *hashKey = vh.GenNewCLKey(curBuf);

	// loop the requested number of times or until canceled. determine if we 
	// found a winner, and send all winners found as solutions. count only one hash. 
	// hashrate is determined by multiplying hash by VERUSHASHES_PER_SOLVE, with VerusHash, only
	// hashrate and sharerate are valid, solutionrate will equal sharerate
	for (int64_t i = 0; i < VERUSHASHES_PER_SOLVE; i++)
	{
		*extraPtr = i;

		// prepare the buffer
		vh.FillExtra((u128 *)curBuf);

		/*
        if (!i)
        {
            std::cout << "pre-buffer = ";
            std::cout << HexBytes(curBuf, 64);

            std::cout << std::endl;
            std::cout << "test_buf = [";
            for (int k = 0; k < 64; k++)
            {
                if (k == 63)
                {
                    std::cout << strprintf("0x%02x]", *(curBuf + k));
                }
                else
                {
                    std::cout << strprintf("0x%02x, ", *(curBuf + k));
                }
            }
            std::cout << std::endl;

            std::cout << "test_key = [";
            for (int k = 0; k < (((u128 *)hasherrefresh) - hashKey); k++)
            {
                std::cout << "0x";
                std::cout << LEToHex(*(hashKey + k));
                if (k == (((u128 *)hasherrefresh) - hashKey) - 1)
                {
                    std::cout << "]";
                }
                else
                {
                    std::cout << ", ";
                }
            }
            std::cout << std::endl;
        }
		*/

		// run verusclhash on the buffer
		intermediate = vclh(curBuf, hashKey, pMoveScratch);

		// fill buffer to the end with the result and final hash
		vh.FillExtra(&intermediate);
		(*vh.haraka512KeyedFunction)((unsigned char *)&curHash, curBuf, hashKey + vh.IntermediateTo128Offset(intermediate));

		/*
        if (!i)
        {
            std::cout << "intermediate: ";
            std::cout << LEToHex(intermediate);
            std::cout << std::endl;
            std::cout << "hashBytes: ";
            std::cout << HexBytes((unsigned char *)&curHash, 32);
            std::cout << std::endl;
        }
		*/

		if (UintToArith256(curHash) > target)
        {
            // refresh the key
            memcpy(hashKey, hasherrefresh, keyrefreshsize);
			continue;
        }

		int extraSpace = (solution.size() % 32) + 15;
		assert(solution.size() > 32);
		*((int64_t *)&(solution.data()[solution.size() - extraSpace])) = i;

		solutionf(std::vector<uint32_t>(0), solution.size(), solution.data());
		if (cancelf()) return;
        memcpy(hashKey, hasherrefresh, keyrefreshsize);
	}

	hashdonef();
}

