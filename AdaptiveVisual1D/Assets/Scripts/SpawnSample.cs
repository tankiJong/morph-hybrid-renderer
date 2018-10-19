using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Events;

[System.Serializable]
public class SampleFloatEvent : UnityEvent<float> {
	
}
public class SpawnSample : MonoBehaviour {
	public GameObject target;
	// Use this for initialization
	public float interval = 1f;
	
	[SerializeField]
	public SampleFloatEvent OnSample;

	void Start () {
		StartCoroutine(spawnInterval());
	}

	IEnumerator spawnInterval() {
		while (true) {
			yield return new WaitForSeconds(interval);
			spawnTarget();
		}
	}
	
	// Update is called once per frame
	void spawnTarget () {
		GameObject obj = Instantiate(target);
		obj.transform.position = transform.position;
		OnSample.Invoke(transform.position.y);
	}
}
