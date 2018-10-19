using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class DieAfterSeconds : MonoBehaviour {

	// Use this for initialization
	void Start () {
		StartCoroutine(dieAfter(20f));
	}
	
	// Update is called once per frame
	void Update () {
		
	}


	IEnumerator dieAfter(float second) {
		yield return new WaitForSeconds(second);
		Destroy(gameObject);
	}
}
