// ----------------------------------------------------------------------------
//  OgmaNeo
//  Copyright(c) 2016-2020 Ogma Intelligent Systems Corp. All rights reserved.
//
//  This copy of OgmaNeo is licensed to you under the terms described
//  in the OGMANEO_LICENSE.md file included in this distribution.
// ----------------------------------------------------------------------------

#include "Helpers.h"

#include "ComputeSystem.h"

using namespace ogmaneo;

void ogmaneo::runKernel1(
    ComputeSystem &cs,
    const std::function<void(int, std::mt19937 &)> &func,
    int size,
    std::mt19937 &rng,
    int batchSize
) {
    std::uniform_int_distribution<int> seedDist(0, 999999);

    // Ceil divide
    int batches = (size + batchSize - 1) / batchSize;

    std::vector<std::future<void>> futures(batches);

    // Create work items
    for (int x = 0; x < batches; x++) {
        int itemBatchSize = std::min(size - x * batchSize, batchSize);
        
        futures[x] = std::move(cs.pool.push([](int id, int seed, int pos, int batchSize, const std::function<void(int, std::mt19937 &)> &func) {
            std::mt19937 subRng(seed);

            for (int x = 0; x < batchSize; x++)
                func(pos + x, subRng);
        }, seedDist(rng), x * batchSize, itemBatchSize, func));
    }
    
    // Wait
    for (int i = 0 ; i < futures.size(); i++)
        futures[i].wait();
}

void ogmaneo::runKernel2(
    ComputeSystem &cs,
    const std::function<void(const Int2 &, std::mt19937 &)> &func,
    const Int2 &size, std::mt19937 &rng,
    const Int2 &batchSize
) {
    std::uniform_int_distribution<int> seedDist(0, 999999);

    // Ceil divide
    Int2 batches((size.x + batchSize.x - 1) / batchSize.x, (size.y + batchSize.y - 1) / batchSize.y);

    std::vector<std::future<void>> futures(batches.x * batches.y);

    // Create work items
    for (int x = 0; x < batches.x; x++)
        for (int y = 0; y < batches.y; y++) {
            Int2 itemBatchSize = Int2(std::min(size.x - x * batchSize.x, batchSize.x), std::min(size.y - y * batchSize.y, batchSize.y));

            futures[x + y * batches.x] = std::move(cs.pool.push([](int id, int seed, const Int2 &pos, const Int2 &batchSize, const std::function<void(const Int2 &, std::mt19937 &)> &func) {
                std::mt19937 subRng(seed);

                for (int x = 0; x < batchSize.x; x++)
                    for (int y = 0; y < batchSize.y; y++) {
                        Int2 bPos;
                        bPos.x = pos.x + x;
                        bPos.y = pos.y + y;

                        func(bPos, subRng);
                    }
            }, seedDist(rng), Int2(x * batchSize.x, y * batchSize.y), itemBatchSize, func));
        }

    // Wait
    for (int i = 0 ; i < futures.size(); i++)
        futures[i].wait();
}

void ogmaneo::runKernel3(
    ComputeSystem &cs,
    const std::function<void(const Int3 &, std::mt19937 &)> &func,
    const Int3 &size,
    std::mt19937 &rng,
    const Int3 &batchSize
) {
    std::uniform_int_distribution<int> seedDist(0, 999999);

    // Ceil divide
    Int3 batches((size.x + batchSize.x - 1) / batchSize.x, (size.y + batchSize.y - 1) / batchSize.y, (size.z + batchSize.z - 1) / batchSize.z);

    std::vector<std::future<void>> futures(batches.x * batches.y * batches.z);

    // Create work items
    for (int x = 0; x < batches.x; x++)
        for (int y = 0; y < batches.y; y++) 
            for (int z = 0; z < batches.z; z++) {
                Int3 itemBatchSize = Int3(std::min(size.x - x * batchSize.x, batchSize.x), std::min(size.y - y * batchSize.y, batchSize.y), std::min(size.z - z * batchSize.z, batchSize.z));

                futures[x + y * batches.x + z * batches.x * batches.y] = std::move(cs.pool.push([](int id, int seed, const Int3 &pos, const Int3 &batchSize, const std::function<void(const Int3 &, std::mt19937 &)> &func) {
                    std::mt19937 subRng(seed);

                    for (int x = 0; x < batchSize.x; x++)
                        for (int y = 0; y < batchSize.y; y++)
                            for (int z = 0; z < batchSize.z; z++) {
                                Int3 bPos;
                                bPos.x = pos.x + x;
                                bPos.y = pos.y + y;
                                bPos.z = pos.z + z;

                                func(bPos, subRng);
                            }
                }, seedDist(rng), Int3(x * batchSize.x, y * batchSize.y, z * batchSize.z), itemBatchSize, func));
            }

    // Wait
    for (int i = 0 ; i < futures.size(); i++)
        futures[i].wait();
}

void ogmaneo::fillInt(
    int pos,
    std::mt19937 &rng,
    IntBuffer* buffer,
    int fillValue
) {
    (*buffer)[pos] = fillValue;
}

void ogmaneo::fillFloat(
    int pos,
    std::mt19937 &rng,
    FloatBuffer* buffer,
    float fillValue
) {
    (*buffer)[pos] = fillValue;
}

void ogmaneo::copyInt(
    int pos,
    std::mt19937 &rng,
    const IntBuffer* src,
    IntBuffer* dst
) {
    (*dst)[pos] = (*src)[pos];
}

void ogmaneo::copyFloat(
    int pos,
    std::mt19937 &rng,
    const FloatBuffer* src,
    FloatBuffer* dst
) {
    (*dst)[pos] = (*src)[pos];
}

std::vector<IntBuffer*> ogmaneo::get(
    std::vector<std::shared_ptr<IntBuffer>> &v
) {
    std::vector<IntBuffer*> vp(v.size());

    for (int i = 0; i < v.size(); i++)
        vp[i] = v[i].get();

    return vp;
}

std::vector<FloatBuffer*> ogmaneo::get(
    std::vector<std::shared_ptr<FloatBuffer>> &v
) {
    std::vector<FloatBuffer*> vp(v.size());

    for (int i = 0; i < v.size(); i++)
        vp[i] = v[i].get();

    return vp;
}

std::vector<const IntBuffer*> ogmaneo::constGet(
    const std::vector<std::shared_ptr<IntBuffer>> &v
) {
    std::vector<const IntBuffer*> vp(v.size());

    for (int i = 0; i < v.size(); i++)
        vp[i] = v[i].get();

    return vp;
}

std::vector<const FloatBuffer*> ogmaneo::constGet(
    const std::vector<std::shared_ptr<FloatBuffer>> &v
) {
    std::vector<const FloatBuffer*> vp(v.size());

    for (int i = 0; i < v.size(); i++)
        vp[i] = v[i].get();

    return vp;
}

std::vector<IntBuffer*> ogmaneo::get(
    std::vector<IntBuffer> &v
) {
    std::vector<IntBuffer*> vp(v.size());

    for (int i = 0; i < v.size(); i++)
        vp[i] = &v[i];

    return vp;
}

std::vector<FloatBuffer*> ogmaneo::get(
    std::vector<FloatBuffer> &v
) {
    std::vector<FloatBuffer*> vp(v.size());

    for (int i = 0; i < v.size(); i++)
        vp[i] = &v[i];

    return vp;
}

std::vector<const IntBuffer*> ogmaneo::constGet(
    const std::vector<IntBuffer> &v
) {
    std::vector<const IntBuffer*> vp(v.size());

    for (int i = 0; i < v.size(); i++)
        vp[i] = &v[i];

    return vp;
}

std::vector<const FloatBuffer*> ogmaneo::constGet(
    const std::vector<FloatBuffer> &v
) {
    std::vector<const FloatBuffer*> vp(v.size());

    for (int i = 0; i < v.size(); i++)
        vp[i] = &v[i];

    return vp;
}

void ogmaneo::initSMLocalRF(
    const Int3 &inSize,
    const Int3 &outSize,
    int radius,
    SparseMatrix &mat
) {
    int numOut = outSize.x * outSize.y * outSize.z;

    // Projection constant
    Float2 outToIn = Float2(static_cast<float>(inSize.x) / static_cast<float>(outSize.x),
        static_cast<float>(inSize.y) / static_cast<float>(outSize.y));

    int diam = radius * 2 + 1;

    int numWeightsPerOutput = diam * diam * inSize.z;

    int weightsSize = numOut * numWeightsPerOutput;

    mat.nonZeroValues.reserve(weightsSize);

    mat.rowRanges.resize(numOut + 1);

    mat.columnIndices.reserve(weightsSize);

    // Initialize weight matrix
    for (int ox = 0; ox < outSize.x; ox++)
        for (int oy = 0; oy < outSize.y; oy++) {
            Int2 visiblePositionCenter = project(Int2(ox, oy), outToIn);

            // Lower corner
            Int2 fieldLowerBound(visiblePositionCenter.x - radius, visiblePositionCenter.y - radius);

            // Bounds of receptive field, clamped to input size
            Int2 iterLowerBound(std::max(0, fieldLowerBound.x), std::max(0, fieldLowerBound.y));
            Int2 iterUpperBound(std::min(inSize.x - 1, visiblePositionCenter.x + radius), std::min(inSize.y - 1, visiblePositionCenter.y + radius));

            for (int oz = 0; oz < outSize.z; oz++) {
                Int3 outPos(ox, oy, oz);

                int nonZeroInRow = 0;

                for (int ix = iterLowerBound.x; ix <= iterUpperBound.x; ix++)
                    for (int iy = iterLowerBound.y; iy <= iterUpperBound.y; iy++) {
                        for (int iz = 0; iz < inSize.z; iz++) {
                            Int3 inPos(ix, iy, iz);

                            int inIndex = address3(inPos, inSize);

                            mat.nonZeroValues.push_back(0.0f);
                            mat.columnIndices.push_back(inIndex);
                            
                            nonZeroInRow++;
                        }
                    }

                mat.rowRanges[address3(outPos, outSize)] = nonZeroInRow;
            }
        }

    mat.nonZeroValues.shrink_to_fit();
    mat.columnIndices.shrink_to_fit();

    // Convert rowRanges from counts to cumulative counts
    int offset = 0;

	for (int i = 0; i < numOut; i++) {
		int temp = mat.rowRanges[i];

		mat.rowRanges[i] = offset;

		offset += temp;
	}

    mat.rowRanges[numOut] = offset;

    mat.rows = numOut;
    mat.columns = inSize.x * inSize.y * inSize.z;
}

void ogmaneo::writeSMToStream(
    std::ostream &os,
    const SparseMatrix &mat
) {
    os.write(reinterpret_cast<const char*>(&mat.rows), sizeof(int));
    os.write(reinterpret_cast<const char*>(&mat.columns), sizeof(int));

    writeBufferToStream(os, &mat.nonZeroValues);
    writeBufferToStream(os, &mat.nonZeroValueIndices);
    writeBufferToStream(os, &mat.rowRanges);
    writeBufferToStream(os, &mat.columnIndices);
    writeBufferToStream(os, &mat.columnRanges);
    writeBufferToStream(os, &mat.rowIndices);
}

void ogmaneo::readSMFromStream(
    std::istream &is,
    SparseMatrix &mat
) {
    is.read(reinterpret_cast<char*>(&mat.rows), sizeof(int));
    is.read(reinterpret_cast<char*>(&mat.columns), sizeof(int));

    readBufferFromStream(is, &mat.nonZeroValues);
    readBufferFromStream(is, &mat.nonZeroValueIndices);
    readBufferFromStream(is, &mat.rowRanges);
    readBufferFromStream(is, &mat.columnIndices);
    readBufferFromStream(is, &mat.columnRanges);
    readBufferFromStream(is, &mat.rowIndices);
}