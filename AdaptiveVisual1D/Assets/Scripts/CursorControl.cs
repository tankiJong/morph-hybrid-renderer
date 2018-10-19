using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Events;

public class CursorControl : MonoBehaviour {
	public float ShiftSpeed = 3f;
	public float ExpandShrinkSpeed = 1f;
	public LineRenderer line;
	// Use this for initialization

	public GameObject sample;
	public GameObject Integration;
	public float SpawnSampleInterval;
	
	void Start () {
	}
	
	float sampleRange = 0;
	// Update is called once per frame

	public float range() {
		return sampleRange;
	}

	public float center() {
		return transform.position.y;
	}
	
	void Update () {
		float vertical = Input.GetAxis("Vertical");
		transform.Translate(0, ShiftSpeed * vertical * Time.deltaTime, 0);

		float horizontal = Input.GetAxis("Horizontal");
		Vector3 p0 = line.GetPosition(0);
		p0.y -= ExpandShrinkSpeed * horizontal * Time.deltaTime;
		line.SetPosition(0, p0);
		
		Vector3 p1 = line.GetPosition(1);
		p1.y += ExpandShrinkSpeed * horizontal * Time.deltaTime;
		line.SetPosition(1, p1);

		sampleRange = p1.y - p0.y;
		updateSample();
	}

	void updateSample() {
		Vector3 pos = sample.transform.localPosition;
		pos.y = Gussian.Next(transform.localPosition.y, sampleRange * .25f);
		sample.transform.localPosition = pos;
	}
	
}
