using UnityEngine;
using System.Collections;
using System.Collections.Generic;

[ExecuteInEditMode]
public class BezierCurve : MonoBehaviour {
    public Vector3 start;
    public Vector3 end;

    public Vector3 tangentStart;
    public Vector3 tangentEnd;

    [Range(0, 1)]public float force = 0;
    [Range(0, 1)] public float smooth = 0;
    [Range(0, 5)] public float scale = 1;
    private LineRenderer lr;
    private void Start() {
        lr = GetComponent<LineRenderer>();
    }
    
    private Vector3 rotatePointAroundPivot(Vector3 point, Vector3 pivot, Vector3 angles) {
        return Quaternion.Euler(angles) * (point - pivot) + pivot;
    }

    // accept uniformed scaled value [0, 1]
    public float setForce(float f) {
        force = force * .5f + Mathf.Clamp01(f) * .5f;

        return force;
    }
    
    // accept uniformed scaled value [0, 1]
    public float setSmooth(float f) {
        smooth = Mathf.Clamp01(f) * .5f + smooth * .5f;
        return smooth;
    }
    
    // accept uniformed scaled value [0, 1]
    public float setScale(float f) {
        float val = 5 * Mathf.Clamp01(f);
        scale = val * .8f + scale * .2f;
        return scale;
    }
    private void Update() {
        tangentStart.x =  1.5f * force;
        tangentStart.y = 0;
        
        tangentEnd.x = 1 - force;
        tangentEnd.y = 1;
        
        tangentStart = rotatePointAroundPivot(tangentStart, Vector3.zero, new Vector3(0, 0, 45 * smooth));
        tangentEnd = rotatePointAroundPivot(tangentEnd, Vector3.one, new Vector3(0, 0, 45 * smooth));
        
        Queue<Vector3> pos = new Queue<Vector3>();
        float t = 0;
        float step = .01f;

        pos.Enqueue(evaluate(t));
        do {
            t += step;
            Vector3 curr = evaluate(t);
            pos.Enqueue(curr);
        } while (t <= 1);

        lr.positionCount = pos.Count;
        lr.SetPositions(pos.ToArray());
    }

    public Vector3 evaluate(float t) {
//        if (t > 1) {
//            Debug.LogError("t out of range");
//        }

        var a = Vector3.Lerp(start, tangentStart, t);
        var b = Vector3.Lerp(tangentStart, tangentEnd, t);
        var c = Vector3.Lerp(tangentEnd, end, t);

        var d = Vector3.Lerp(a, b, t);
        var e = Vector3.Lerp(b, c, t);

        var re = Vector3.Lerp(d, e, t);

        re.y = Mathf.Pow(re.y, scale);

        return re;
    }

    private void OnDrawGizmos() {

    }
}