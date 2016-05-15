// 创建异步访问对象
function createXHR() 
{
    var xhr;
    try 
    {
        xhr = new ActiveXObject("Msxml2.XMLHTTP");
    }
    catch (e) 
    {
        try 
        {
            xhr = new ActiveXObject("Microsoft.XMLHTTP");
        }
        catch (E) 
        {
            xhr = false;
        }
    }
    if (!xhr && typeof XMLHttpRequest != 'undefined') 
    {
        xhr = new XMLHttpRequest();
    }
    return xhr;
}

// 异步访问提交处理
function sender() 
{
    xhr = createXHR();
    if (xhr)
    {
        xhr.onreadystatechange=callbackFunction;
        //test.cgi后面跟个cur_time参数是为了防止Ajax页面缓存
        xhr.open("GET", "cgi-bin/pycgi_disptime.py?cur_time=" + new Date().getTime());
        xhr.send(null);
    }
    else
    {
        //XMLHttpRequest对象创建失败
        alert("浏览器不支持，请更换浏览器！");
    }
}

// 异步回调函数处理
function callbackFunction()
{
    if (xhr.readyState == 4) 
    {
        if (xhr.status == 200) 
        {
            var returnValue = xhr.responseText;

            if (returnValue != null && returnValue.length > 0)
            {
                document.getElementById("current_time").innerHTML = returnValue;
            }
            else
            {
                alert("结果为空！");
            }
        } 
        else 
        {
            alert("页面出现异常！");
        }
    }
}