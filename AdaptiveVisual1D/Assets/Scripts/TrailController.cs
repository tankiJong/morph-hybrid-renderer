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
	[Range(32, 128)] public uint WindowSize = 32;
	
	private SampleRingBuffer buffer;

	private uint sampleSpend = 0;
	private bool isConveraged = false;

	[SerializeField] public CONVERAGE_METHOD method = CONVERAGE_METHOD.MOVING_AVERAGE;
	
	private void Start() {
		buffer = new SampleRingBuffer(WindowSize);
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
		
		Vector3 position = transform.position;

		switch (method) {
			case CONVERAGE_METHOD.EXPONENTIAL_MOVING_AVERAGE:
				position.y = exponentialMovingAverage(position.y, buffer.average());
				break;
			case CONVERAGE_METHOD.MOVING_AVERAGE:
				position.y = movingAverage(buffer.average());
				break;
		}
		
		transform.position = position;
	}

	float exponentialMovingAverage(float original, float average) {
		return original * .9f + average * .1f;
	}
	
	float movingAverage(float average) {
		return average;
	}

	float adaptiveAverage(float original, float average, float variance) {
		return original;
	}
	
}
