using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.UI;


class SampleRingBuffer {
	private uint BUFFER_SIZE;
	private uint nextToWrite = 0;
	private float[] samples;

	public SampleRingBuffer(uint size) {
		BUFFER_SIZE = size;
		samples = new float[BUFFER_SIZE];
	}
	public void write(float val) {
		samples[nextToWrite % BUFFER_SIZE] = val;
		nextToWrite++;
	}

	public uint size() {
		return nextToWrite % BUFFER_SIZE == nextToWrite ? nextToWrite : BUFFER_SIZE;
	}

	public float average() {
		uint size = this.size();
		if (size == 0) return 0;
		
		float sum = 0;
		for (uint i = 0; i < size; i++) {
			sum += samples[i];
		}
	
		return sum / size;
	}

	public float variance() {
		if (size() == 0) return 0;
		float avg = average();

		float sum = 0;
		for (uint i = 0; i < size(); i++) {
			sum += (samples[i] - avg) * (samples[i] - avg);
		}
		sum /= size();

		return sum;
	}

	public float min() {
		return samples.Min();
	}

	public float max() {
		return samples.Max();
	}

	public float range() {
		if (size() == 0) return 1;
		return max() - min();
	}

	public uint dataIndex(uint abstractIndex) {
		if (abstractIndex >= BUFFER_SIZE) {
			Debug.LogError("index out of bounds");
		}
		
		if (nextToWrite < BUFFER_SIZE) return abstractIndex;
		
		// zero is next to Writ
		// uint zeroIndex = nextToWrite % BUFFER_SIZE;
		return (nextToWrite + abstractIndex) % BUFFER_SIZE;
	}

	public float varianceEffectedAverage() {
		if (size() == 0) return 0;

		float pureAverage = average();

		float sum = 0;
		float weight = 0;
		for (uint i = 0; i < size(); i++) {
			float wei = Mathf.Abs(samples[i] - pureAverage) / range();
			wei = 1 - wei * wei;
			weight += wei;
			sum += samples[i] * wei;
		}

		float vari = variance();

		float k = Mathf.Clamp01(vari / range());
//		Debug.Log("k: " + k);
		float avg = pureAverage * k + sum / weight * (1 - k);
		return avg;
	}

	public float weightedAverage(BezierCurve weightCurve) {
		float sum = 0;
		float weight = 0;
		for (uint i = 0; i < size(); i++) {
			float wei = weightCurve.evaluate((float)i / (float) BUFFER_SIZE).y;
			sum += samples[dataIndex(i)] * wei;
			weight += wei;
		}

		Debug.Log("Weighted average: " + sum / weight);
		return sum / weight;
	}
}

[Serializable]
public enum CONVERAGE_METHOD {
	MOVING_AVERAGE,
	EXPONENTIAL_MOVING_AVERAGE,
	ADAPTIVE,
}

public class TrailController : MonoBehaviour {
	public Text numSampleSpendToConverageText;
	public Text VarianceText;
	public CursorControl context;
	public float varianceThrehold = .2f;
	[Range(8, 128)] public uint WindowSize = 32;
	public BezierCurve WeightCurve;
	public bool AccountLightMovemt = true;
	private SampleRingBuffer buffer;
	private SampleRingBuffer varianceBuffer;
	private uint sampleSpend = 0;
	private bool isConveraged = false;

	[SerializeField] public CONVERAGE_METHOD method = CONVERAGE_METHOD.MOVING_AVERAGE;
	
	private void Start() {
		buffer = new SampleRingBuffer(WindowSize);
		varianceBuffer = new SampleRingBuffer(WindowSize);
	}

	private void Update() {
		bool newConveraged = Mathf.Abs(transform.position.y - context.center()) < varianceThrehold;

		if (newConveraged != isConveraged) {
			if (newConveraged) {
				if(numSampleSpendToConverageText)
					numSampleSpendToConverageText.text = "#Sample: " + sampleSpend;
			}
			else {
				if(numSampleSpendToConverageText)
					numSampleSpendToConverageText.text = "Converaging...";
				sampleSpend = 0;
			}
		}
		
		isConveraged = newConveraged;
		if(VarianceText)
			VarianceText.text = "Variance: " + buffer.variance().ToString("F3");
	}

	public void onSample(float val) {
		if (!isConveraged) {
			sampleSpend++;
		}
		buffer.write(val);
		varianceBuffer.write(buffer.variance());
		Vector3 position = transform.position;

		switch (method) {
			case CONVERAGE_METHOD.EXPONENTIAL_MOVING_AVERAGE:
				position.y = exponentialMovingAverage(position.y, val, .01f);
				break;
			case CONVERAGE_METHOD.MOVING_AVERAGE:
				position.y = movingAverage(buffer.average());
				break;
			case CONVERAGE_METHOD.ADAPTIVE:
				Debug.Log("variance variance:" + varianceBuffer.variance());
				if (buffer.range() != 0) {
					WeightCurve.setForce( Mathf.Abs(buffer.variance() / buffer.range()) );
				}

				if (varianceBuffer.range() != 0) {
					// WeightCurve.setSmooth( 1 - Mathf.Abs(varianceBuffer.variance() / varianceBuffer.range()) );
					WeightCurve.setScale( Mathf.Abs(varianceBuffer.variance() / varianceBuffer.range()) );
				}
//				if ((context.isMoving() && AccountLightMovemt) || varianceBuffer.variance() > .1f) {
//				position.y = exponentialMovingAverage(position.y, buffer.weightedAverage(WeightCurve));
				position.y = adaptiveAverage(position.y, buffer.weightedAverage(WeightCurve), buffer.variance() / buffer.range());
				// position.y = buffer.weightedAverage(WeightCurve);
//				}
//				else {
//					float adaptive = adaptiveAverage(position.y, buffer.varianceEffectedAverage(), buffer.variance() / buffer.range());
//					float exp = exponentialMovingAverage(position.y, buffer.varianceEffectedAverage(), .01f);
//
//					float k = buffer.variance();
//					position.y = exp;
//				}
				break;
		}
		
		transform.position = position;
	}

	float exponentialMovingAverage(float original, float data, float factor = .1f) {
		return original * (1 - factor) + data * factor;
	}
	
	float movingAverage(float average) {
		return average;
	}

	float adaptiveAverage(float original, float average, float variance) {
		float diff =  Mathf.Abs(variance);
		// Debug.Log("Original: " + original + "|Diff: " + diff);
		float k = Mathf.Clamp01(diff);
		k = Mathf.SmoothStep(0, 0.5f, k);
		return (original * (k) + average * ( 1f - k ));
	}
	
}
