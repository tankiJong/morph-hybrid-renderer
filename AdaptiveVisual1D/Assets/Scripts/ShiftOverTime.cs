using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class ShiftOverTime : MonoBehaviour {
	public float speedSecond = 3f;
	// Use this for initialization
	void Start () {
		
	}
	
	// Update is called once per frame
	void Update () {
		transform.Translate(speedSecond * Time.deltaTime, 0, 0);
	}
}
