// ----------------------------------------------------------------------------
//  OgmaNeo
//  Copyright(c) 2016-2019 Ogma Intelligent Systems Corp. All rights reserved.
//
//  This copy of OgmaNeo is licensed to you under the terms described
//  in the OGMANEO_LICENSE.md file included in this distribution.
// ----------------------------------------------------------------------------

// ------------------------------------------- Common -------------------------------------------

// MWC64X
uint rand(
    uint2 *state
) {
    enum { A = 4294883355U };

    uint x = (*state).x, c = (*state).y;
    
    uint res = x ^ c;

    uint hi = mul_hi(x, A);

    x = x * A + c;

    c = hi + (x < c);

    *state = (uint2)(x, c);

    return res;
}

float randFloat(
    uint2* state
) {
    uint tmp = rand(state);

    return (tmp % 99999) / 99999.0f;
}

float randNormal(
    uint2* state
) {
    float u1 = randFloat(state);
    float u2 = randFloat(state);

    return sqrt(-2.0f * log(u1)) * cos(6.28318f * u2);
}

bool inBounds0(
    int2 position,
    int2 upperBound
) {
    return position.x >= 0 && position.x < upperBound.x && position.y >= 0 && position.y < upperBound.y;
}

bool inBounds(
    int2 position,
    int2 lowerBound,
    int2 upperBound
) {
    return position.x >= lowerBound.x && position.x < upperBound.x && position.y >= lowerBound.y && position.y < upperBound.y;
}

int address2(
    int2 pos,
    int2 dim
) {
    return pos.y + pos.x * dim.y;
}

int address3(
    int3 pos,
    int3 dims
) {
    return pos.z + pos.y * dims.z + pos.x * dims.y * dims.z;
}

int address4(
    int4 pos,
    int4 dims
) {
    return pos.w + pos.z * dims.w + pos.y * dims.z * dims.w + pos.x * dims.y * dims.z * dims.w;
}

inline float sigmoid(
    float x
) {
    if (x < 0.0f) {
        x = exp(x);

        return x / (1.0f + x);
    }
    
    return 1.0f / (1.0f + exp(-x));
}

float multiplyOHVs(
    global const float* nonZeroValues,
    global const int* rowRanges,
    global const int* columnIndices,
    global const int* nonZeroIndices,
    int row,
    int oneHotSize
) {
    float sum = 0.0f;

	int nextIndex = row + 1;
	
	for (int jj = rowRanges[row]; jj < rowRanges[nextIndex]; jj += oneHotSize) {
		int j = jj + nonZeroIndices[columnIndices[jj] / oneHotSize];

		sum += nonZeroValues[j];
	}

    return sum;
}

float multiplyOHVsT(
    global const float* nonZeroValues,
    global const int* columnRanges,
    global const int* rowIndices,
    global const int* nonZeroValueIndices,
    global const int* nonZeroIndices,
    int column,
    int oneHotSize
) {
    float sum = 0.0f;

	int nextIndex = column + 1;
	
	for (int jj = columnRanges[column]; jj < columnRanges[nextIndex]; jj += oneHotSize) {
		int j = jj + nonZeroIndices[rowIndices[jj] / oneHotSize];

		sum += nonZeroValues[nonZeroValueIndices[j]];
	}

    return sum;
}

void deltaOHVs(
    global float* nonZeroValues,
    global const int* rowRanges,
    global const int* columnIndices,
    global const int* nonZeroIndices,
    float delta,
    int row,
    int oneHotSize
) {
	int nextIndex = row + 1;
	
	for (int jj = rowRanges[row]; jj < rowRanges[nextIndex]; jj += oneHotSize) {
		int j = jj + nonZeroIndices[columnIndices[jj] / oneHotSize];

		nonZeroValues[j] += delta;
	}
}

void deltaOHVsT(
    global float* nonZeroValues,
    global const int* columnRanges,
    global const int* rowIndices,
    global const int* nonZeroValueIndices,
    global const int* nonZeroIndices,
    float delta,
    int column,
    int oneHotSize
) {
    int nextIndex = column + 1;

	for (int jj = columnRanges[column]; jj < columnRanges[nextIndex]; jj += oneHotSize) {
		int j = jj + nonZeroIndices[rowIndices[jj] / oneHotSize];

		nonZeroValues[nonZeroValueIndices[j]] += delta;
    }
}

float multiply(
	global const float* nonZeroValues,
    global const int* rowRanges,
    global const int* columnIndices,
    global const float* inputs,
	int row
) {
	float sum = 0.0f;

	int nextIndex = row + 1;
	
	for (int j = rowRanges[row]; j < rowRanges[nextIndex]; j++)
		sum += nonZeroValues[j] * inputs[columnIndices[j]];

	return sum;
}

float distance2(
	global const float* nonZeroValues,
    global const int* rowRanges,
    global const int* columnIndices,
    global const float* inputs,
	int row
) {
	float sum = 0.0f;

	int nextIndex = row + 1;
	
	for (int j = rowRanges[row]; j < rowRanges[nextIndex]; j++) {
        float delta = inputs[columnIndices[j]] - nonZeroValues[j];

		sum += delta * delta;
    }

	return sum;
}

void hebb(
	global float* nonZeroValues,
    global const int* rowRanges,
    global const int* columnIndices,
    global const float* inputs,
	int row,
    float alpha
) {
	int nextIndex = row + 1;
	
	for (int j = rowRanges[row]; j < rowRanges[nextIndex]; j++)
		nonZeroValues[j] += alpha * (inputs[columnIndices[j]] - nonZeroValues[j]);
}

int count(
    global const int* rowRanges,
	int row
) {
	int nextIndex = row + 1;
	
	return rowRanges[nextIndex] - rowRanges[row];
}

int countT(
    global const int* columnRanges,
	int column
) {
	int nextIndex = column + 1;
	
	return columnRanges[nextIndex] - columnRanges[column];
}

float total(
	global const float* nonZeroValues,
    global const int* rowRanges,
	int row
) {
	float sum = 0.0f;

	int nextIndex = row + 1;
	
	for (int j = rowRanges[row]; j < rowRanges[nextIndex]; j++)
		sum += nonZeroValues[j];

	return sum;
}

// ------------------------------------------- Sparse Coder -------------------------------------------

void kernel scForward(
    global const int* visibleCs,
    global float* hiddenActivations,
    global const float* nonZeroValues,
    global const int* rowRanges,
    global const int* columnIndices,
    int3 visibleSize,
    int3 hiddenSize
) {
    int3 hiddenPosition = (int3)(get_global_id(0), get_global_id(1), get_global_id(2));

    int hiddenIndex = address3(hiddenPosition, hiddenSize);

    hiddenActivations[hiddenIndex] += multiplyOHVs(nonZeroValues, rowRanges, columnIndices, visibleCs, hiddenIndex, visibleSize.z);
}

void kernel scInhibit(
    global const float* hiddenActivations,
    global int* hiddenCs,
    int3 hiddenSize
) {
    int2 hiddenColumnPosition = (int2)(get_global_id(0), get_global_id(1));

    int maxIndex = 0;
    float maxValue = hiddenActivations[address3((int3)(hiddenColumnPosition, 0), hiddenSize)];
    
    // Find max
    for (int c = 1; c < hiddenSize.z; c++) {
        float value = hiddenActivations[address3((int3)(hiddenColumnPosition, c), hiddenSize)];

        if (value > maxValue) {
            maxValue = value;
            maxIndex = c;
        }
    }

    // Set states
    hiddenCs[address2(hiddenColumnPosition, hiddenSize.xy)] = maxIndex;
}

void kernel scLearn(
    global const int* visibleCs,
    global const int* hiddenCs,
    global float* nonZeroValues,
    global const int* nonZeroValueIndices,
    global const int* columnRanges,
    global const int* rowIndices,
    int3 visibleSize,
    int3 hiddenSize,
    float alpha
) {
    int2 visibleColumnPosition = (int2)(get_global_id(0), get_global_id(1));

    int visibleColumnIndex = address2(visibleColumnPosition, visibleSize.xy);

    int maxIndex = 0;
    float maxValue = -999999.0f;

    for (int c = 0; c < visibleSize.z; c++) {
        int visibleIndex = address3((int3)(visibleColumnPosition, c), visibleSize);

        float sum = multiplyOHVsT(nonZeroValues, columnRanges, rowIndices, nonZeroValueIndices, hiddenCs, visibleIndex, hiddenSize.z);

        if (sum > maxValue) {
            maxValue = sum;

            maxIndex = c;
        }
    }

    int visibleC = visibleCs[visibleColumnIndex];

    if (maxIndex != visibleC) {
        for (int c = 0; c < visibleSize.z; c++) {
            int visibleIndex = address3((int3)(visibleColumnPosition, c), visibleSize);

            float sum = multiplyOHVsT(nonZeroValues, columnRanges, rowIndices, nonZeroValueIndices, hiddenCs, visibleIndex, hiddenSize.z);

            sum /= max(1, countT(columnRanges, visibleIndex) / hiddenSize.z);

            float delta = alpha * ((c == visibleC ? 1.0f : 0.0f) - (sum > 0.0f ? 1.0f + sum : exp(sum)));

            deltaOHVsT(nonZeroValues, columnRanges, rowIndices, nonZeroValueIndices, hiddenCs, delta, visibleIndex, hiddenSize.z);
        }
    }
}

// ------------------------------------------- Predictor -------------------------------------------

void kernel pCount(
    global const int* rowRanges,
    global int* hiddenCounts,
    int3 visibleSize,
    int3 hiddenSize
) {
    int2 hiddenColumnPosition = (int2)(get_global_id(0), get_global_id(1));
	      
    int hiddenColumnIndex = address2(hiddenColumnPosition, hiddenSize.xy);

    int hiddenIndex = address3((int3)(hiddenColumnPosition, 0), hiddenSize);

    hiddenCounts[hiddenColumnIndex] += count(rowRanges, hiddenIndex) / visibleSize.z;
}

void kernel pForward(
    global const int* visibleCs,
    global float* hiddenActivations,
    global const float* nonZeroValues,
    global const int* rowRanges,
    global const int* columnIndices,
    int3 visibleSize,
    int3 hiddenSize
) {
    int3 hiddenPosition = (int3)(get_global_id(0), get_global_id(1), get_global_id(2));
	      
    int hiddenColumnIndex = address2(hiddenPosition.xy, hiddenSize.xy);

    int hiddenIndex = address3(hiddenPosition, hiddenSize);
    
    hiddenActivations[hiddenIndex] += multiplyOHVs(nonZeroValues, rowRanges, columnIndices, visibleCs, hiddenIndex, visibleSize.z);
}

void kernel pInhibit(
    global const float* hiddenActivations,
    global int* hiddenCs,
    int3 hiddenSize
) {
    int2 hiddenColumnPosition = (int2)(get_global_id(0), get_global_id(1));

    int maxIndex = 0;
    float maxValue = hiddenActivations[address3((int3)(hiddenColumnPosition, 0), hiddenSize)];

    for (int c = 1; c < hiddenSize.z; c++) {
        float value = hiddenActivations[address3((int3)(hiddenColumnPosition, c), hiddenSize)];

        if (value > maxValue) {
            maxValue = value;
            maxIndex = c;
        }
    }

    // Set states
    hiddenCs[address2(hiddenColumnPosition, hiddenSize.xy)] = maxIndex;
}

void kernel pLearn(
    global const int* visibleCsPrev,
    global const float* hiddenActivationsPrev,
    global const int* hiddenTargetCs,
    global const int* hiddenCounts,
    global float* nonZeroValues,
    global const int* rowRanges,
    global const int* columnIndices,
    int3 visibleSize,
    int3 hiddenSize,
    float alpha
) {
    int3 hiddenPosition = (int3)(get_global_id(0), get_global_id(1), get_global_id(2));
	
    int hiddenColumnIndex = address2(hiddenPosition.xy, hiddenSize.xy);

    float rescale = 1.0f / max(1, hiddenCounts[hiddenColumnIndex]);

    int hiddenIndex = address3(hiddenPosition, hiddenSize);

    int hiddenTargetC = hiddenTargetCs[hiddenColumnIndex];

    float delta = (hiddenPosition.z == hiddenTargetC ? 1.0f : -1.0f) - tanh(hiddenActivationsPrev[hiddenIndex] * rescale);

    deltaOHVs(nonZeroValues, rowRanges, columnIndices, visibleCsPrev, alpha * delta, hiddenIndex, visibleSize.z);
}

// ------------------------------------------- Actor -------------------------------------------

void kernel aCount(
    global const int* rowRanges,
    global int* hiddenCounts,
    int3 visibleSize,
    int3 hiddenSize
) {
    int2 hiddenColumnPosition = (int2)(get_global_id(0), get_global_id(1));
	      
    int hiddenColumnIndex = address2(hiddenColumnPosition, hiddenSize.xy);

    int hiddenIndex = address3((int3)(hiddenColumnPosition, 0), (int3)(hiddenSize.xy, hiddenSize.z + 1));

    hiddenCounts[hiddenColumnIndex] += count(rowRanges, hiddenIndex) / visibleSize.z;
}

void kernel aForward(
    global const int* visibleCs,
    global float* hiddenValues,
    global float* hiddenActivations,
    global const float* nonZeroValues,
    global const int* rowRanges,
    global const int* columnIndices,
    int3 visibleSize,
    int3 hiddenSize
) {
    int3 hiddenPosition = (int3)(get_global_id(0), get_global_id(1), get_global_id(2));
	      
    int hiddenIndex1 = address3(hiddenPosition, (int3)(hiddenSize.xy, hiddenSize.z + 1));

    // If is value
    if (hiddenPosition.z == hiddenSize.z)
        hiddenValues[address2(hiddenPosition.xy, hiddenSize.xy)] += multiplyOHVs(nonZeroValues, rowRanges, columnIndices, visibleCs, hiddenIndex1, visibleSize.z);
    else
        hiddenActivations[address3(hiddenPosition, hiddenSize)] += multiplyOHVs(nonZeroValues, rowRanges, columnIndices, visibleCs, hiddenIndex1, visibleSize.z);
}

void kernel aActivate(
    global float* hiddenActivations,
    global const int* hiddenCounts,
    int3 hiddenSize
) {
    int2 hiddenColumnPosition = (int2)(get_global_id(0), get_global_id(1));

    float rescale = 1.0f / max(1, hiddenCounts[address2(hiddenColumnPosition, hiddenSize.xy)]);

    float maxValue = hiddenActivations[address3((int3)(hiddenColumnPosition, 0), hiddenSize)] * rescale;
 
    // Find max
    for (int c = 1; c < hiddenSize.z; c++)
        maxValue = fmax(maxValue, hiddenActivations[address3((int3)(hiddenColumnPosition, c), hiddenSize)] * rescale);

    float total = 0.0f;

    for (int c = 0; c < hiddenSize.z; c++)
        total += exp(hiddenActivations[address3((int3)(hiddenColumnPosition, c), hiddenSize)] * rescale - maxValue);

    for (int c = 0; c < hiddenSize.z; c++) {
        int hiddenIndex = address3((int3)(hiddenColumnPosition, c), hiddenSize);

        hiddenActivations[hiddenIndex] = exp(hiddenActivations[hiddenIndex] * rescale - maxValue) / fmax(0.0001f, total);
    }
}

void kernel aInhibit(
    global const float* hiddenActivations,
    global int* hiddenCs,
    int3 hiddenSize,
    uint2 seed
) {
    int2 hiddenColumnPosition = (int2)(get_global_id(0), get_global_id(1));

    uint2 stateValue = seed + (uint2)(get_global_id(0) * 293 + 12443, get_global_id(1) * 136 + 235) * 5461;

    int selectIndex = 0;

    float cusp = randFloat(&stateValue);

    float sumSoFar = 0.0f;

    for (int c = 0; c < hiddenSize.z; c++) {
        sumSoFar += hiddenActivations[address3((int3)(hiddenColumnPosition, c), hiddenSize)];

        if (sumSoFar >= cusp) {
            selectIndex = c;

            break;
        }
    }

    // Set states
    hiddenCs[address2(hiddenColumnPosition, hiddenSize.xy)] = selectIndex;
}

void kernel aLearn(
    global const int* visibleCsPrev,
    global const float* hiddenValues,
    global const float* hiddenValuesPrev,
    global const float* hiddenValuesPrevPrev,
    global const float* hiddenActivationsPrev,
    global const int* hiddenCsPrev,
    global const int* hiddenCounts,
    global float* nonZeroValues,
    global const int* rowRanges,
    global const int* columnIndices,
    int3 visibleSize,
    int3 hiddenSize,
    float alpha,
    float beta,
    float g,
    float q
) {
    int3 hiddenPosition = (int3)(get_global_id(0), get_global_id(1), get_global_id(2));
	
    int hiddenColumnIndex = address2(hiddenPosition.xy, hiddenSize.xy);

    int hiddenCPrev = hiddenCsPrev[hiddenColumnIndex];

    float rescale = 1.0f / max(1, hiddenCounts[hiddenColumnIndex]);

    float qUpdate = q + g * hiddenValues[hiddenColumnIndex] * rescale;
    
    int hiddenIndex1 = address3(hiddenPosition, (int3)(hiddenSize.xy, hiddenSize.z + 1));

    if (hiddenPosition.z == hiddenSize.z) {
        float tdError = qUpdate - hiddenValuesPrev[hiddenColumnIndex] * rescale;

        float update = alpha * tdError;

        deltaOHVs(nonZeroValues, rowRanges, columnIndices, visibleCsPrev, update, hiddenIndex1, visibleSize.z);
    }
    else {
        int hiddenIndex = address3(hiddenPosition, hiddenSize);

        float tdErrorPrev = qUpdate - hiddenValuesPrevPrev[hiddenColumnIndex] * rescale;
        
        float update = (tdErrorPrev > 0.0f ? beta : -beta) * (hiddenPosition.z == hiddenCPrev ? 1.0f - hiddenActivationsPrev[hiddenIndex] : -hiddenActivationsPrev[hiddenIndex]);
        
        deltaOHVs(nonZeroValues, rowRanges, columnIndices, visibleCsPrev, update, hiddenIndex1, visibleSize.z);
    }
}

// ------------------------------------------- Image Encoder -------------------------------------------

void kernel imForward(
    global const float* visibleActivations,
    global float* hiddenActivations,
    global const float* nonZeroValues,
    global const int* rowRanges,
    global const int* columnIndices,
    int3 hiddenSize
) {
    int3 hiddenPosition = (int3)(get_global_id(0), get_global_id(1), get_global_id(2));

    int hiddenIndex = address3(hiddenPosition, hiddenSize);

    hiddenActivations[hiddenIndex] -= distance2(nonZeroValues, rowRanges, columnIndices, visibleActivations, hiddenIndex);
}

void kernel imInhibit(
    global const float* hiddenActivations,
    global int* hiddenCs,
    int3 hiddenSize
) {
    int2 hiddenColumnPosition = (int2)(get_global_id(0), get_global_id(1));

    int maxIndex = 0;
    float maxValue = -999999.0f;
    
    // Find max
    for (int c = 0; c < hiddenSize.z; c++) {
        int hiddenIndex = address3((int3)(hiddenColumnPosition, c), hiddenSize);

        float value = hiddenActivations[hiddenIndex];

        if (value > maxValue) {
            maxValue = value;
            maxIndex = c;
        }
    }

    // Set states
    hiddenCs[address2(hiddenColumnPosition, hiddenSize.xy)] = maxIndex;
}

void kernel imLearn(
    global const float* visibleActivations,
    global const int* hiddenCs,
    global const float* hiddenResources,
    global float* nonZeroValues,
    global const int* rowRanges,
    global const int* columnIndices,
    int3 visibleSize,
    int3 hiddenSize,
    float alpha,
    float gamma
) {
    int2 hiddenColumnPosition = (int2)(get_global_id(0), get_global_id(1));

    int hiddenColumnIndex = address2(hiddenColumnPosition, hiddenSize.xy);

    int hiddenC = hiddenCs[hiddenColumnIndex];
    
    for (int c = 0; c < hiddenSize.z; c++) {
        int hiddenIndex = address3((int3)(hiddenColumnPosition, c), hiddenSize);

        float delta = hiddenC - c;

        float strength = exp(-delta * delta * gamma / fmax(0.001f, hiddenResources[hiddenIndex])) * hiddenResources[hiddenIndex];

        hebb(nonZeroValues, rowRanges, columnIndices, visibleActivations, hiddenIndex, strength);
    }
}

void kernel imDeplete(
    global const int* hiddenCs,
    global float* hiddenResources,
    int3 hiddenSize,
    float alpha,
    float gamma
) {
    int2 hiddenColumnPosition = (int2)(get_global_id(0), get_global_id(1));

    int hiddenColumnIndex = address2(hiddenColumnPosition, hiddenSize.xy);

    int hiddenC = hiddenCs[hiddenColumnIndex];

    for (int c = 0; c < hiddenSize.z; c++) {
        int hiddenIndex = address3((int3)(hiddenColumnPosition, c), hiddenSize);

        float delta = hiddenC - c;

        float strength = exp(-delta * delta * gamma / fmax(0.001f, hiddenResources[hiddenIndex])) * hiddenResources[hiddenIndex];

        hiddenResources[hiddenIndex] -= alpha * strength;
    }
}
