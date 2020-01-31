// ----------------------------------------------------------------------------
//  OgmaNeo
//  Copyright(c) 2016-2020 Ogma Intelligent Systems Corp. All rights reserved.
//
//  This copy of OgmaNeo is licensed to you under the terms described
//  in the OGMANEO_LICENSE.md file included in this distribution.
// ----------------------------------------------------------------------------

#include "SparseCoder.h"

using namespace ogmaneo;

void SparseCoder::forward(
    const Int2 &pos,
    std::mt19937 &rng,
    const std::vector<const IntBuffer*> &inputCs,
    int it
) {
    int hiddenColumnIndex = address2(pos, Int2(hiddenSize.x, hiddenSize.y));

    int maxIndex = 0;
    float maxActivation = -999999.0f;

    for (int hc = 0; hc < hiddenSize.z; hc++) {
        int hiddenIndex = address3(Int3(pos.x, pos.y, hc), hiddenSize);

        float sum = 0.0f;

        // For each visible layer
        for (int vli = 0; vli < visibleLayers.size(); vli++) {
            VisibleLayer &vl = visibleLayers[vli];
            const VisibleLayerDesc &vld = visibleLayerDescs[vli];

            if (it == 0)
                sum += vl.weights.multiplyOHVs(*inputCs[vli], hiddenIndex, vld.size.z);
            else
                sum += vl.weights.multiplyOHVs(*inputCs[vli], vl.inputErrors, hiddenIndex, vld.size.z);
        }

        if (it == 0)
            hiddenActivations[hiddenIndex] = sum;
        else
            hiddenActivations[hiddenIndex] += sum;

        if (hiddenActivations[hiddenIndex] > maxActivation) {
            maxActivation = hiddenActivations[hiddenIndex];
            maxIndex = hc;
        }
    }

    hiddenCs[hiddenColumnIndex] = maxIndex;
}

void SparseCoder::backward(
    const Int2 &pos,
    std::mt19937 &rng,
    const IntBuffer* inputCs,
    int vli
) {
    VisibleLayer &vl = visibleLayers[vli];
    VisibleLayerDesc &vld = visibleLayerDescs[vli];

    int visibleColumnIndex = address2(pos, Int2(vld.size.x, vld.size.y));

    int targetC = (*inputCs)[visibleColumnIndex];

    int visibleIndex = address3(Int3(pos.x, pos.y, targetC), vld.size);

    float activation = vl.weights.multiplyOHVsT(hiddenCs, visibleIndex, hiddenSize.z) / std::max(1, vl.weights.countT(visibleIndex) / hiddenSize.z);

    vl.inputErrors[visibleColumnIndex] = std::max(0.0f, 1.0f - std::tanh(activation));
}

void SparseCoder::learn(
    const Int2 &pos,
    std::mt19937 &rng,
    const IntBuffer* inputCs,
    int vli
) {
    VisibleLayer &vl = visibleLayers[vli];
    VisibleLayerDesc &vld = visibleLayerDescs[vli];

    int visibleColumnIndex = address2(pos, Int2(vld.size.x, vld.size.y));

    int targetC = (*inputCs)[visibleColumnIndex];

    int maxIndex = 0;
    float maxActivation = -999999.0f;
    std::vector<float> activations(vld.size.z);

    for (int vc = 0; vc < vld.size.z; vc++) {
        int visibleIndex = address3(Int3(pos.x, pos.y, vc), vld.size);

        float sum = vl.weights.multiplyOHVsT(hiddenCs, visibleIndex, hiddenSize.z) / std::max(1, vl.weights.countT(visibleIndex) / hiddenSize.z);

        activations[vc] = sum;

        if (sum > maxActivation) {
            maxActivation = sum;

            maxIndex = vc;
        }
    }

    if (maxIndex != targetC) {
        for (int vc = 0; vc < vld.size.z; vc++) {
            int visibleIndex = address3(Int3(pos.x, pos.y, vc), vld.size);

            float delta = alpha * ((vc == targetC ? 1.0f : -1.0f) - std::tanh(activations[vc]));

            vl.weights.deltaOHVsT(hiddenCs, delta, visibleIndex, hiddenSize.z);
        }
    }
}

void SparseCoder::initRandom(
    ComputeSystem &cs,
    const Int3 &hiddenSize,
    const std::vector<VisibleLayerDesc> &visibleLayerDescs
) {
    this->visibleLayerDescs = visibleLayerDescs;

    this->hiddenSize = hiddenSize;

    visibleLayers.resize(visibleLayerDescs.size());

    // Pre-compute dimensions
    int numHiddenColumns = hiddenSize.x * hiddenSize.y;
    int numHidden = numHiddenColumns * hiddenSize.z;

    std::uniform_real_distribution<float> weightDist(0.0f, 1.0f);

    // Create layers
    for (int vli = 0; vli < visibleLayers.size(); vli++) {
        VisibleLayer &vl = visibleLayers[vli];
        VisibleLayerDesc &vld = this->visibleLayerDescs[vli];

        int numVisibleColumns = vld.size.x * vld.size.y;
        int numVisible = numVisibleColumns * vld.size.z;

        // Create weight matrix for this visible layer and initialize randomly
        initSMLocalRF(vld.size, hiddenSize, vld.radius, vl.weights);

        for (int i = 0; i < vl.weights.nonZeroValues.size(); i++)
            vl.weights.nonZeroValues[i] = weightDist(cs.rng);

        // Generate transpose (needed for reconstruction)
        vl.weights.initT();

        vl.inputErrors = FloatBuffer(numVisibleColumns, 0.0f);
    }

    // Hidden Cs
    hiddenCs = IntBuffer(numHiddenColumns, 0);

    hiddenActivations = FloatBuffer(numHidden, 0.0f);
}

void SparseCoder::step(
    ComputeSystem &cs,
    const std::vector<const IntBuffer*> &inputCs,
    bool learnEnabled
) {
    int numHiddenColumns = hiddenSize.x * hiddenSize.y;
    int numHidden = numHiddenColumns * hiddenSize.z;

    for (int it = 0; it < explainIters; it++) {
#ifdef KERNEL_NO_THREAD
        for (int x = 0; x < hiddenSize.x; x++)
            for (int y = 0; y < hiddenSize.y; y++)
                forward(Int2(x, y), cs.rng, inputCs, it);
#else
        runKernel2(cs, std::bind(SparseCoder::forwardKernel, std::placeholders::_1, std::placeholders::_2, this, inputCs, it), Int2(hiddenSize.x, hiddenSize.y), cs.rng, cs.batchSize2);
#endif

        if (it < explainIters - 1) {
            for (int vli = 0; vli < visibleLayers.size(); vli++) {
                VisibleLayer &vl = visibleLayers[vli];
                VisibleLayerDesc &vld = visibleLayerDescs[vli];

#ifdef KERNEL_NO_THREAD
                for (int x = 0; x < vld.size.x; x++)
                    for (int y = 0; y < vld.size.y; y++)
                        backward(Int2(x, y), cs.rng, inputCs[vli], vli);
#else
                runKernel2(cs, std::bind(SparseCoder::backwardKernel, std::placeholders::_1, std::placeholders::_2, this, inputCs[vli], vli), Int2(vld.size.x, vld.size.y), cs.rng, cs.batchSize2);
#endif
            }
        }
    }

    if (learnEnabled) {
        for (int vli = 0; vli < visibleLayers.size(); vli++) {
            VisibleLayer &vl = visibleLayers[vli];
            VisibleLayerDesc &vld = visibleLayerDescs[vli];

#ifdef KERNEL_NO_THREAD
            for (int x = 0; x < vld.size.x; x++)
                for (int y = 0; y < vld.size.y; y++)
                    learn(Int2(x, y), cs.rng, inputCs[vli], vli);
#else
            runKernel2(cs, std::bind(SparseCoder::learnKernel, std::placeholders::_1, std::placeholders::_2, this, inputCs[vli], vli), Int2(vld.size.x, vld.size.y), cs.rng, cs.batchSize2);
#endif
        }
    }
}

void SparseCoder::writeToStream(
    std::ostream &os
) const {
    int numHiddenColumns = hiddenSize.x * hiddenSize.y;
    int numHidden = numHiddenColumns * hiddenSize.z;

    os.write(reinterpret_cast<const char*>(&hiddenSize), sizeof(Int3));

    os.write(reinterpret_cast<const char*>(&explainIters), sizeof(int));
    os.write(reinterpret_cast<const char*>(&alpha), sizeof(float));

    writeBufferToStream(os, &hiddenCs);

    int numVisibleLayers = visibleLayers.size();

    os.write(reinterpret_cast<char*>(&numVisibleLayers), sizeof(int));
    
    for (int vli = 0; vli < visibleLayers.size(); vli++) {
        const VisibleLayer &vl = visibleLayers[vli];
        const VisibleLayerDesc &vld = visibleLayerDescs[vli];

        os.write(reinterpret_cast<const char*>(&vld), sizeof(VisibleLayerDesc));

        writeSMToStream(os, vl.weights);
    }
}

void SparseCoder::readFromStream(
    std::istream &is
) {
    is.read(reinterpret_cast<char*>(&hiddenSize), sizeof(Int3));

    int numHiddenColumns = hiddenSize.x * hiddenSize.y;
    int numHidden = numHiddenColumns * hiddenSize.z;

    is.read(reinterpret_cast<char*>(&explainIters), sizeof(int));
    is.read(reinterpret_cast<char*>(&alpha), sizeof(float));

    readBufferFromStream(is, &hiddenCs);

    hiddenActivations = FloatBuffer(numHidden, 0.0f);

    int numVisibleLayers;
    
    is.read(reinterpret_cast<char*>(&numVisibleLayers), sizeof(int));

    visibleLayers.resize(numVisibleLayers);
    visibleLayerDescs.resize(numVisibleLayers);
    
    for (int vli = 0; vli < visibleLayers.size(); vli++) {
        VisibleLayer &vl = visibleLayers[vli];
        VisibleLayerDesc &vld = visibleLayerDescs[vli];

        is.read(reinterpret_cast<char*>(&vld), sizeof(VisibleLayerDesc));

        int numVisibleColumns = vld.size.x * vld.size.y;
        int numVisible = numVisibleColumns * vld.size.z;

        readSMFromStream(is, vl.weights);

        vl.inputErrors = FloatBuffer(numVisibleColumns, 0.0f);
    }
}