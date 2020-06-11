// (C) 2018 Michael Toutonghi
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#pragma once 

#include "primitives/block.h"
#include "../nheqminer/hash.h"

struct cpu_verushash
{
	std::string getdevinfo() { return ""; }

	static void start(cpu_verushash& device_context);

	static void stop(cpu_verushash& device_context);

	static void solve_verus(CBlockHeader &bh, 
		arith_uint256 &target,
		std::function<bool()> cancelf,
		std::function<void(const std::vector<uint32_t>&, size_t, const unsigned char*)> solutionf,
		std::function<void(void)> hashdonef,
		cpu_verushash &device_context);

	static void solve_verus_v2(CBlockHeader &bh, 
		arith_uint256 &target,
		std::function<bool()> cancelf,
		std::function<void(const std::vector<uint32_t>&, size_t, const unsigned char*)> solutionf,
		std::function<void(void)> hashdonef,
		cpu_verushash &device_context);

	static void solve_verus_v2_opt(CBlockHeader &bh, 
		arith_uint256 &target,
		std::function<bool()> cancelf,
		std::function<void(const std::vector<uint32_t>&, size_t, const unsigned char*)> solutionf,
		std::function<void(void)> hashdonef,
		cpu_verushash &device_context);

	std::string getname()
	{ 
		return "VerusHash - CPU";
	}

	cpu_verushash(int solutionVer = SOLUTION_VERUSHHASH_V2) : solutionVer(solutionVer) {}

	CVerusHashWriter *pVHW;
	CVerusHashV2bWriter *pVHW2b;
	int solutionVer;
	int use_opt; // unused
};

